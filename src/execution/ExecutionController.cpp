#include "execution/ExecutionController.h"

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QLabel>
#include "ui/IconUtils.h"
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSize>
#include <QSplitter>
#include <QStyle>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <Qsci/qsciscintilla.h>
#include <csignal>

ExecutionController::ExecutionController(QObject *parent)
    : QObject(parent),
      compilerProcess_(new QProcess(this)),
      runProcess_(new QProcess(this)) {

    timeoutTimer_ = new QTimer(this);
    timeoutTimer_->setSingleShot(true);
    connect(timeoutTimer_, &QTimer::timeout, this, [this]() {
        if (state_ != State::Running) {
            return;
        }
        timedOut_ = true;
        stopRequested_ = false;
        if (runProcess_->state() != QProcess::NotRunning) {
            runProcess_->kill();
            runProcess_->waitForFinished(1000);
        }
        lastExecutionTimeMs_ = timeoutMs_;
        updateStatus("Time Limit Exceeded");
        if (ui_.errorViewer) {
            ui_.errorViewer->setPlainText("Time Limit Exceeded");
        }
        updateOutputPanels(false, true);
        setState(State::Idle);
        cleanupTempDir();
        emit executionFinished(QString(), "Time Limit Exceeded", -1);
    });
    
    // Compiler process signals
    connect(compilerProcess_, &QProcess::finished,
            this, &ExecutionController::onCompilationFinished);

    // Run process signals
    connect(runProcess_, &QProcess::finished,
            this, &ExecutionController::onRunFinished);
    connect(runProcess_, &QProcess::errorOccurred,
            this, &ExecutionController::onRunError);
    
    // Send input when process starts
    connect(runProcess_, &QProcess::started, this, [this]() {
        if (ui_.inputEditor) {
            const QString input = ui_.inputEditor->toPlainText();
            if (!input.isEmpty()) {
                runProcess_->write(input.toUtf8());
            }
        }
        runProcess_->closeWriteChannel();
    });
}

ExecutionController::~ExecutionController() {
    // Kill any running processes
    if (compilerProcess_->state() != QProcess::NotRunning) {
        compilerProcess_->kill();
        compilerProcess_->waitForFinished(1000);
    }
    if (runProcess_->state() != QProcess::NotRunning) {
        runProcess_->kill();
        runProcess_->waitForFinished(1000);
    }
    // Ensure temp directory is cleaned up
    cleanupTempDir();
}

void ExecutionController::bind(const UiBindings &bindings) {
    ui_ = bindings;
    updateRunButtonForState(state_);
}

void ExecutionController::runWithBindings(const UiBindings &bindings) {
    ui_ = bindings;
    updateRunButtonForState(state_);
    
    if (state_ != State::Idle) {
        return;
    }
    
    startCompilation();
}

void ExecutionController::stop() {
    if (state_ == State::Idle) {
        return;
    }

    stopRequested_ = true;
    if (timeoutTimer_->isActive()) {
        timeoutTimer_->stop();
    }

    if (compilerProcess_->state() != QProcess::NotRunning) {
        compilerProcess_->kill();
        compilerProcess_->waitForFinished(1000);
    }
    if (runProcess_->state() != QProcess::NotRunning) {
        runProcess_->kill();
        runProcess_->waitForFinished(1000);
    }

    updateStatus("Stopped");
    setState(State::Idle);
    cleanupTempDir();
}

void ExecutionController::setState(State newState) {
    if (state_ != newState) {
        state_ = newState;
        emit stateChanged(newState);
    }
    updateRunButtonForState(newState);
}

void ExecutionController::updateRunButtonForState(State newState) {
    if (!ui_.runButton) {
        return;
    }

    const bool stopMode = (newState == State::Compiling || newState == State::Running);
    ui_.runButton->setProperty("stop", stopMode);

    if (stopMode) {
        ui_.runButton->setIcon(QIcon());
        ui_.runButton->setText(QString(QChar(0x25A0)));
        ui_.runButton->setToolTip("Stop");
    } else {
        ui_.runButton->setText(QString());
        ui_.runButton->setIcon(
            IconUtils::makeTintedIcon(":/images/play.svg", iconColor_, QSize(16, 16)));
        ui_.runButton->setIconSize(QSize(16, 16));
        ui_.runButton->setToolTip("Compile and Run");
    }

    ui_.runButton->style()->unpolish(ui_.runButton);
    ui_.runButton->style()->polish(ui_.runButton);
    ui_.runButton->update();
}

void ExecutionController::setIconTintColor(const QColor &color) {
    iconColor_ = color;
    updateRunButtonForState(state_);
}

QString ExecutionController::normalizedLanguage() const {
    const QString normalized = language_.trimmed().toLower();
    if (normalized == "python" || normalized == "py") {
        return "Python";
    }
    if (normalized == "java") {
        return "Java";
    }
    return "C++";
}

QStringList ExecutionController::splitArgs(const QString &args) const {
    const QString trimmed = args.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return QProcess::splitCommand(trimmed);
}

QString ExecutionController::detectJavaMainClass(const QString &code) const {
    const QRegularExpression publicClassPattern(
        "\\bpublic\\s+class\\s+([A-Za-z_][A-Za-z0-9_]*)\\b");
    QRegularExpressionMatch match = publicClassPattern.match(code);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    const QRegularExpression classPattern(
        "\\bclass\\s+([A-Za-z_][A-Za-z0-9_]*)\\b");
    match = classPattern.match(code);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return "Solution";
}

void ExecutionController::startCompilation() {
    setState(State::Compiling);
    updateStatus("Compiling...");
    clearOutputs();
    stopRequested_ = false;
    lastExecutionTimeMs_ = -1;
    timedOut_ = false;

    cleanupTempDir();
    tempDir_ = std::make_unique<QTemporaryDir>();
    if (!tempDir_ || !tempDir_->isValid()) {
        updateStatus("Compile Error");
        if (ui_.errorViewer) {
            ui_.errorViewer->setPlainText("Failed to create a temporary build directory.");
        }
        updateOutputPanels(false, true);
        setState(State::Idle);
        return;
    }

    const QString solution = ui_.codeEditor ? ui_.codeEditor->text() : QString();
    const QString code = applyTransclusion(solution);
    const QString language = normalizedLanguage();
    const QString tempPath = tempDir_->path();
    const QString sourceExt = language == "Python" ? "py"
                           : language == "Java"   ? "java"
                                                   : "cpp";
    const QString sourceBaseName =
        language == "Java" ? detectJavaMainClass(code) : "solution";
    const QString sourcePath =
        QDir(tempPath).filePath(QString("%1.%2").arg(sourceBaseName, sourceExt));
#ifdef Q_OS_WIN
    const QString outputPath = QDir(tempPath).filePath("solution.exe");
#else
    const QString outputPath = QDir(tempPath).filePath("solution");
#endif

    runProgram_.clear();
    runArgs_.clear();
    QFile file(sourcePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << code;
        file.close();
    } else {
        updateStatus("Compile Error");
        if (ui_.errorViewer) {
            ui_.errorViewer->setPlainText("Failed to write source file to temporary directory.");
        }
        updateOutputPanels(false, true);
        setState(State::Idle);
        cleanupTempDir();
        return;
    }



    if (language == "Python") {
#ifdef Q_OS_WIN
        const QString defaultPython = "python";
#else
        const QString defaultPython = "python3";
#endif
        runProgram_ = pythonPath_.trimmed().isEmpty() ? defaultPython : pythonPath_.trimmed();
        runArgs_ = splitArgs(pythonArgs_);
        runArgs_ << sourcePath;
        emit compilationSucceeded();
        startExecution();
        return;
    }

    if (language == "Java") {
        runProgram_ = javaRunPath_.trimmed().isEmpty() ? "java" : javaRunPath_.trimmed();
        runArgs_ = splitArgs(javaArgs_);
        runArgs_ << "-cp" << tempPath << sourceBaseName;
    } else {
        runProgram_ = outputPath;
    }

    // Kill any running compilation
    if (compilerProcess_->state() != QProcess::NotRunning) {
        compilerProcess_->kill();
        compilerProcess_->waitForFinished(1000);
    }

    compilerProcess_->setWorkingDirectory(tempPath);
    if (language == "Java") {
        const QString javacPath =
            javaCompilerPath_.trimmed().isEmpty() ? "javac" : javaCompilerPath_.trimmed();
        compilerProcess_->start(javacPath, {sourcePath});
    } else {
        const QString compilerPath =
            cppCompilerPath_.trimmed().isEmpty() ? "g++" : cppCompilerPath_.trimmed();
        QStringList compileArgs = splitArgs(cppCompilerFlags_);
        compileArgs << sourcePath << "-o" << outputPath;
        compilerProcess_->start(compilerPath, compileArgs);
    }
}

void ExecutionController::onCompilationFinished(int exitCode, QProcess::ExitStatus status) {
    if (stopRequested_) {
        stopRequested_ = false;
        cleanupTempDir();
        setState(State::Idle);
        return;
    }
    if (exitCode == 0 && status == QProcess::NormalExit) {
        emit compilationSucceeded();
        startExecution();
    } else {
        const QString error = QString::fromUtf8(compilerProcess_->readAllStandardError());
        updateStatus("Compile Error");
        if (ui_.errorViewer) {
            ui_.errorViewer->setPlainText(error);
        }
        updateOutputPanels(false, !error.isEmpty());
        
        setState(State::Idle);
        emit compilationFailed(error);
        cleanupTempDir();
    }
}

void ExecutionController::startExecution() {
    setState(State::Running);
    updateStatus("Running...");
    runTimer_.start();
    timedOut_ = false;
    if (timeoutTimer_->isActive()) {
        timeoutTimer_->stop();
    }
    if (timeoutMs_ > 0) {
        timeoutTimer_->start(timeoutMs_);
    }
    


    if (!tempDir_ || !tempDir_->isValid()) {
        updateStatus("Run Failed");
        if (ui_.errorViewer) {
            ui_.errorViewer->setPlainText("Temporary build directory is missing.");
        }
        updateOutputPanels(false, true);
        setState(State::Idle);
        cleanupTempDir();
        return;
    }

    // Kill any running execution
    if (runProcess_->state() != QProcess::NotRunning) {
        runProcess_->kill();
        runProcess_->waitForFinished(1000);
    }

    if (runProgram_.isEmpty()) {
        updateStatus("Run Failed");
        if (ui_.errorViewer) {
            ui_.errorViewer->setPlainText("Execution command is not configured.");
        }
        updateOutputPanels(false, true);
        setState(State::Idle);
        cleanupTempDir();
        return;
    }

    runProcess_->setWorkingDirectory(tempDir_->path());
    runProcess_->start(runProgram_, runArgs_);
}

void ExecutionController::onRunFinished(int exitCode, QProcess::ExitStatus status) {
    if (timeoutTimer_->isActive()) {
        timeoutTimer_->stop();
    }
    if (timedOut_) {
        timedOut_ = false;
        return;
    }
    if (stopRequested_) {
        stopRequested_ = false;
        cleanupTempDir();
        setState(State::Idle);
        return;
    }
    lastExecutionTimeMs_ = runTimer_.elapsed();
    const QString stdOut = QString::fromUtf8(runProcess_->readAllStandardOutput());
    const QString stdErr = QString::fromUtf8(runProcess_->readAllStandardError());
    QString effectiveErr = stdErr;

    if (status == QProcess::CrashExit) {
        QString signalName;
        switch (exitCode) {
        case SIGSEGV:
            signalName = "SIGSEGV";
            break;
        case SIGABRT:
            signalName = "SIGABRT";
            break;
        case SIGFPE:
            signalName = "SIGFPE";
            break;
        case SIGILL:
            signalName = "SIGILL";
            break;
#ifdef SIGBUS
        case SIGBUS:
            signalName = "SIGBUS";
            break;
#endif
#ifdef SIGTRAP
        case SIGTRAP:
            signalName = "SIGTRAP";
            break;
#endif
#ifdef SIGKILL
        case SIGKILL:
            signalName = "SIGKILL";
            break;
#endif
        case SIGTERM:
            signalName = "SIGTERM";
            break;
        case SIGINT:
            signalName = "SIGINT";
            break;
#ifdef SIGPIPE
        case SIGPIPE:
            signalName = "SIGPIPE";
            break;
#endif
#ifdef SIGALRM
        case SIGALRM:
            signalName = "SIGALRM";
            break;
#endif
#ifdef SIGXCPU
        case SIGXCPU:
            signalName = "SIGXCPU";
            break;
#endif
#ifdef SIGXFSZ
        case SIGXFSZ:
            signalName = "SIGXFSZ";
            break;
#endif
        default:
            signalName = QString("SIGNAL %1").arg(exitCode);
            break;
        }

        const QString signalLine = QString("Terminated by signal: %1").arg(signalName);
        if (!effectiveErr.isEmpty()) {
            effectiveErr.append('\n');
        }
        effectiveErr.append(signalLine);
    }

    // Update output viewers
    if (ui_.outputViewer) {
        ui_.outputViewer->setPlainText(stdOut);
    }
    if (ui_.errorViewer) {
        ui_.errorViewer->setPlainText(effectiveErr);
    }

    // Determine result status
    QString resultStatus;
    if (status != QProcess::NormalExit || exitCode != 0) {
        resultStatus = "Runtime Error";
    } else {
        const QString expected = ui_.expectedEditor ? ui_.expectedEditor->toPlainText() : QString();
        if (normalizeText(stdOut) == normalizeText(expected)) {
            resultStatus = "Accepted";
        } else {
            resultStatus = "Wrong Answer";
        }
    }
    
    const bool showOutput = !stdOut.isEmpty() || resultStatus != "Accepted";
    const bool showError = !effectiveErr.isEmpty();
    updateOutputPanels(showOutput, showError);

    updateStatus(resultStatus);
    setState(State::Idle);
    cleanupTempDir();
    emit executionFinished(stdOut, stdErr, exitCode);
}

void ExecutionController::onRunError(QProcess::ProcessError /*error*/) {
    if (timeoutTimer_->isActive()) {
        timeoutTimer_->stop();
    }
    if (timedOut_) {
        timedOut_ = false;
        return;
    }
    if (stopRequested_) {
        stopRequested_ = false;
        cleanupTempDir();
        setState(State::Idle);
        return;
    }
    if (runProcess_->error() == QProcess::WriteError) {
        // Ignore broken pipe when the program exits early and closes stdin.
        return;
    }

    if (runTimer_.isValid()) {
        lastExecutionTimeMs_ = runTimer_.elapsed();
    }
    updateStatus("Run Failed");

    if (ui_.errorViewer) {
        const QString errorText = runProcess_->errorString();
        ui_.errorViewer->setPlainText(errorText);
        updateOutputPanels(false, !errorText.isEmpty());
    }

    setState(State::Idle);
    cleanupTempDir();
}

void ExecutionController::cleanupTempDir() {
    tempDir_.reset();
}

void ExecutionController::updateStatus(const QString &status) {
    if (ui_.statusLabel) {
        QString display = status;
        QString color;
        QString timeSuffix;

        if (status == "Accepted") {
            display = "AC";
            color = "#2e7d32";
        } else if (status == "Compile Error") {
            display = "CE";
            color = "#c42b1c";
        } else if (status == "Runtime Error") {
            display = "RE";
            color = "#c42b1c";
        } else if (status == "Time Limit Exceeded") {
            display = "TLE";
            color = "#c42b1c";
        } else if (status == "Wrong Answer") {
            display = "WA";
            color = "#c42b1c";
        }

        if (lastExecutionTimeMs_ >= 0 &&
            (status == "Accepted" ||
             status == "Runtime Error" ||
             status == "Time Limit Exceeded" ||
             status == "Wrong Answer")) {
            timeSuffix = QString(" \u2022 %1 ms").arg(lastExecutionTimeMs_);
        }

        ui_.statusLabel->setText(display + timeSuffix);
        QString style = "font-weight: 700;";
        if (!color.isEmpty()) {
            style.prepend(QString("color: %1; ").arg(color));
        }
        ui_.statusLabel->setStyleSheet(style);
    }
}

void ExecutionController::clearOutputs() {
    if (ui_.outputViewer) {
        ui_.outputViewer->clear();
    }
    if (ui_.errorViewer) {
        ui_.errorViewer->clear();
    }
    updateOutputPanels(false, false);
}

void ExecutionController::updateOutputPanels(bool showOutput, bool showError) {
    if (!ui_.outputSplitter || !ui_.outputBlock || !ui_.errorBlock) {
        return;
    }

    const bool showSplitter = showOutput || showError;
    ui_.outputBlock->setVisible(showOutput);
    ui_.errorBlock->setVisible(showError);
    ui_.outputSplitter->setVisible(showSplitter);

    if (!showSplitter) {
        return;
    }

    // Adjust splitter sizes based on what's visible
    if (showOutput && !showError) {
        ui_.outputSplitter->setSizes({1, 0});
    } else if (!showOutput && showError) {
        ui_.outputSplitter->setSizes({0, 1});
    } else {
        ui_.outputSplitter->setSizes({1, 1});
    }
}

QString ExecutionController::applyTransclusion(const QString &solution) const {
    if (!transcludeTemplate_) {
        return solution;
    }
    // Replace //#main with the solution code
    QString result = template_;
    
    // Find //#main and replace with solution
    // Support both "//#main" on its own line and inline
    int index = result.indexOf("//#main");
    if (index != -1) {
        result.replace("//#main", solution);
    } else {
        // If no //#main marker, just use the solution as-is
        result = solution;
    }
    
    return result;
}

QString ExecutionController::normalizeText(const QString &text) const {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace("\r", "\n");
    QStringList lines = normalized.split('\n');
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
        lines.removeLast();
    }
    for (QString &line : lines) {
        while (line.endsWith(' ') || line.endsWith('\t')) {
            line.chop(1);
        }
    }
    return lines.join('\n');
}
