#include "execution/ParallelExecutor.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QList>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtConcurrent>

ParallelExecutor::ParallelExecutor(QObject *parent)
    : QObject(parent),
      watcher_(new QFutureWatcher<TestResult>(this)) {
    
    connect(watcher_, &QFutureWatcher<TestResult>::resultReadyAt,
            this, [this](int index) {
        if (cancelled_) {
            return;
        }
        const TestResult result = watcher_->resultAt(index);
        if (result.testIndex >= 0 &&
            result.testIndex < expectedResults_) {
            results_[static_cast<size_t>(result.testIndex)] = result;
        }
        emit testFinished(result);
    });

    connect(watcher_, &QFutureWatcher<TestResult>::finished,
            this, [this]() {
        if (!cancelled_) {
            emit allTestsFinished(results_);
        }
        running_ = false;
    });
}

ParallelExecutor::~ParallelExecutor() {
    cancel();
}

void ParallelExecutor::setSourceCode(const QString &code) {
    sourceCode_ = code;
}

void ParallelExecutor::setTemplate(const QString &tmpl) {
    template_ = tmpl.isEmpty() ? "//#main" : tmpl;
}

QString ParallelExecutor::applyTransclusion(const QString &solution) const {
    if (!transcludeTemplate_) {
        return solution;
    }
    if (template_.contains("//#main")) {
        QString result = template_;
        return result.replace("//#main", solution);
    }
    return solution;
}

QString ParallelExecutor::normalizedLanguage() const {
    const QString normalized = language_.trimmed().toLower();
    if (normalized == "python" || normalized == "py") {
        return "Python";
    }
    if (normalized == "java") {
        return "Java";
    }
    return "C++";
}

QString ParallelExecutor::detectJavaMainClass(const QString &code) const {
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

QStringList ParallelExecutor::splitArgs(const QString &args) const {
    const QString trimmed = args.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    return QProcess::splitCommand(trimmed);
}

void ParallelExecutor::runAll(const std::vector<TestInput> &tests) {
    if (running_) {
        return;
    }
    
    running_ = true;
    cancelled_ = false;
    
    emit compilationStarted();
    
    // Run compilation on a background thread to avoid blocking the GUI
    std::vector<TestInput> testsCopy = tests;
    auto compileFuture = QtConcurrent::run([this, testsCopy]() {
        if (!compile()) {
            running_ = false;
            return;
        }

        QMetaObject::invokeMethod(this, [this, testsCopy]() {
            emit compilationFinished(true, QString());

            // Run tests in parallel using QtConcurrent
            QList<TestInput> testList(testsCopy.begin(), testsCopy.end());
            expectedResults_ = static_cast<int>(testsCopy.size());
            results_.assign(static_cast<size_t>(expectedResults_), TestResult{});

            auto future = QtConcurrent::mapped(testList,
                [this](const TestInput &test) -> TestResult {
                    if (cancelled_) {
                        TestResult r;
                        r.testIndex = test.testIndex;
                        r.error = "Cancelled";
                        return r;
                    }
                    return runSingleTest(test);
                });
            watcher_->setFuture(future);
        }, Qt::QueuedConnection);
    });
}

bool ParallelExecutor::compile() {
    // Create temp directory
    tempDir_ = std::make_unique<QTemporaryDir>();
    if (!tempDir_->isValid()) {
        emit compilationFinished(false, "Failed to create temporary directory");
        return false;
    }
    
    const QString tempPath = tempDir_->path();
    const QString code = applyTransclusion(sourceCode_);
    const QString language = normalizedLanguage();
    const QString sourceExt = language == "Python" ? "py"
                           : language == "Java"   ? "java"
                                                   : "cpp";
    const QString sourceBaseName =
        language == "Java" ? detectJavaMainClass(code) : "solution";
    const QString sourcePath =
        QDir(tempPath).filePath(QString("%1.%2").arg(sourceBaseName, sourceExt));
#ifdef Q_OS_WIN
    executablePath_ = QDir(tempPath).filePath("solution.exe");
#else
    executablePath_ = QDir(tempPath).filePath("solution");
#endif

    runProgram_.clear();
    runArgs_.clear();

    QFile file(sourcePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit compilationFinished(false, "Failed to write source file");
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << code;
    file.close();
    
    if (language == "Python") {
#ifdef Q_OS_WIN
        const QString defaultPython = "python";
#else
        const QString defaultPython = "python3";
#endif
        runProgram_ = pythonPath_.trimmed().isEmpty() ? defaultPython : pythonPath_.trimmed();
        runArgs_ = splitArgs(pythonArgs_);
        runArgs_ << sourcePath;
        return true;
    }

    QProcess compiler;
    compiler.setWorkingDirectory(tempPath);

    if (language == "Java") {
        const QString javacPath =
            javaCompilerPath_.trimmed().isEmpty() ? "javac" : javaCompilerPath_.trimmed();
        compiler.start(javacPath, {sourcePath});
    } else {
        QStringList args = splitArgs(compilerFlags_);
        args << sourcePath << "-o" << executablePath_;
        const QString compilerPath =
            compilerPath_.trimmed().isEmpty() ? "g++" : compilerPath_.trimmed();
        compiler.start(compilerPath, args);
    }
    if (!compiler.waitForFinished(30000)) {
        emit compilationFinished(false, "Compilation timed out");
        return false;
    }
    
    if (compiler.exitCode() != 0) {
        const QString error = QString::fromUtf8(compiler.readAllStandardError());
        emit compilationFinished(false, error);
        return false;
    }
    
    if (language == "Java") {
        runProgram_ = javaRunPath_.trimmed().isEmpty() ? "java" : javaRunPath_.trimmed();
        runArgs_ = splitArgs(javaArgs_);
        runArgs_ << "-cp" << tempPath << sourceBaseName;
    } else {
        runProgram_ = executablePath_;
    }

    return true;
}

TestResult ParallelExecutor::runSingleTest(const TestInput &test) {
    TestResult result;
    result.testIndex = test.testIndex;
    
    QElapsedTimer timer;
    timer.start();
    
    if (runProgram_.isEmpty()) {
        result.error = "Execution command is not configured";
        result.exitCode = -1;
        return result;
    }

    QProcess process;
    process.setWorkingDirectory(tempDir_ ? tempDir_->path() : QString());
    process.start(runProgram_, runArgs_);
    
    if (!process.waitForStarted(1000)) {
        result.error = "Failed to start process";
        result.exitCode = -1;
        return result;
    }
    
    // Send input
    process.write(test.input.toUtf8());
    process.closeWriteChannel();
    
    // Wait for completion with timeout
    if (!process.waitForFinished(timeoutMs_)) {
        process.kill();
        process.waitForFinished(1000);
        result.error = "Time Limit Exceeded";
        result.exitCode = -1;
        result.executionTimeMs = timer.elapsed();
        return result;
    }
    
    result.executionTimeMs = timer.elapsed();
    result.exitCode = process.exitCode();
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    
    // Check if output matches expected
    if (result.exitCode == 0 && result.error.isEmpty()) {
        result.passed = (normalizeText(result.output) == normalizeText(test.expectedOutput));
    }
    
    return result;
}

QString ParallelExecutor::normalizeText(const QString &text) const {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace("\r", "\n");
    
    // Remove trailing whitespace from each line and trailing newlines
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

void ParallelExecutor::cancel() {
    cancelled_ = true;
    if (watcher_ && watcher_->isRunning()) {
        watcher_->cancel();
        watcher_->waitForFinished();
    }
    running_ = false;
}
