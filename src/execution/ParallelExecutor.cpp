#include "execution/ParallelExecutor.h"
#include "execution/CompilationUtils.h"

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
#include <csignal>

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
    compileFuture_.waitForFinished();
    if (watcher_) watcher_->waitForFinished();
}

void ParallelExecutor::setSourceCode(const QString &code) {
    sourceCode_ = code;
}



void ParallelExecutor::runAll(const std::vector<TestInput> &tests) {
    if (running_) {
        return;
    }
    
    running_ = true;
    cancelled_ = false;
    
    emit compilationStarted();
    
    // Snapshot shared state so worker threads never touch member fields.
    const QString runProgram = runProgram_;
    const QStringList runArgs = runArgs_;

    // Run compilation on a background thread to avoid blocking the GUI
    std::vector<TestInput> testsCopy = tests;
    compileFuture_ = QtConcurrent::run([this, testsCopy]() {
        if (!compile()) {
            running_ = false;
            return;
        }

        // Snapshot fields set by compile() — these are safe to read here
        // because compile() just finished on this same thread.
        const QString snapshotRunProgram = runProgram_;
        const QStringList snapshotRunArgs = runArgs_;
        const QString snapshotTempPath = tempDir_ ? tempDir_->path() : QString();
        const int snapshotTimeoutMs = timeoutMs_;

        QMetaObject::invokeMethod(this, [this, testsCopy, snapshotRunProgram, snapshotRunArgs, snapshotTempPath, snapshotTimeoutMs]() {
            emit compilationFinished(true, QString());

            // Run tests in parallel using QtConcurrent
            QList<TestInput> testList(testsCopy.begin(), testsCopy.end());
            expectedResults_ = static_cast<int>(testsCopy.size());
            results_.assign(static_cast<size_t>(expectedResults_), TestResult{});

            auto future = QtConcurrent::mapped(testList,
                [this, snapshotRunProgram, snapshotRunArgs, snapshotTempPath, snapshotTimeoutMs](const TestInput &test) -> TestResult {
                    if (cancelled_) {
                        TestResult r;
                        r.testIndex = test.testIndex;
                        r.error = "Cancelled";
                        return r;
                    }
                    return runSingleTest(test, snapshotRunProgram, snapshotRunArgs, snapshotTempPath, snapshotTimeoutMs);
                });
            watcher_->setFuture(future);
        }, Qt::QueuedConnection);
    });
}

bool ParallelExecutor::compile() {
    // Create temp directory
    tempDir_ = std::make_unique<QTemporaryDir>();
    if (!tempDir_->isValid()) {
        QMetaObject::invokeMethod(this, [this]() {
            emit compilationFinished(false, "Failed to create temporary directory");
        }, Qt::QueuedConnection);
        return false;
    }
    
    const QString tempPath = tempDir_->path();
    const QString code = CompilationUtils::applyTransclusion(
        config_.templateCode, sourceCode_, config_.transcludeTemplate);
    const QString language = CompilationUtils::normalizeLanguage(config_.language);
    const QString sourceExt = language == "Python" ? "py"
                           : language == "Java"   ? "java"
                                                   : "cpp";
    const QString sourceBaseName =
        language == "Java" ? CompilationUtils::detectJavaMainClass(code) : "solution";
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
        QMetaObject::invokeMethod(this, [this]() {
            emit compilationFinished(false, "Failed to write source file");
        }, Qt::QueuedConnection);
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
        runProgram_ = config_.pythonPath.trimmed().isEmpty() ? defaultPython : config_.pythonPath.trimmed();
        runArgs_ = CompilationUtils::splitArgs(config_.pythonArgs);
        runArgs_ << sourcePath;
        return true;
    }

    QProcess compiler;
    compiler.setWorkingDirectory(tempPath);

    if (language == "Java") {
        const QString javacPath =
            config_.javaCompilerPath.trimmed().isEmpty() ? "javac" : config_.javaCompilerPath.trimmed();
        compiler.start(javacPath, {sourcePath});
    } else {
        QStringList args = CompilationUtils::splitArgs(config_.cppCompilerFlags);
        args << sourcePath << "-o" << executablePath_;
        const QString compilerPath =
            config_.cppCompilerPath.trimmed().isEmpty() ? "g++" : config_.cppCompilerPath.trimmed();
        compiler.start(compilerPath, args);
    }
    if (!compiler.waitForFinished(30000)) {
        QMetaObject::invokeMethod(this, [this]() {
            emit compilationFinished(false, "Compilation timed out");
        }, Qt::QueuedConnection);
        return false;
    }
    
    if (compiler.exitCode() != 0) {
        const QString error = QString::fromUtf8(compiler.readAllStandardError());
        QMetaObject::invokeMethod(this, [this, error]() {
            emit compilationFinished(false, error);
        }, Qt::QueuedConnection);
        return false;
    }
    
    if (language == "Java") {
        runProgram_ = config_.javaRunPath.trimmed().isEmpty() ? "java" : config_.javaRunPath.trimmed();
        runArgs_ = CompilationUtils::splitArgs(config_.javaArgs);
        runArgs_ << "-cp" << tempPath << sourceBaseName;
    } else {
        runProgram_ = executablePath_;
    }

    return true;
}

TestResult ParallelExecutor::runSingleTest(const TestInput &test,
                                           const QString &program,
                                           const QStringList &args,
                                           const QString &workDir,
                                           int timeoutMs) {
    TestResult result;
    result.testIndex = test.testIndex;
    
    QElapsedTimer timer;
    timer.start();
    
    if (program.isEmpty()) {
        result.error = "Execution command is not configured";
        result.exitCode = -1;
        return result;
    }

    QProcess process;
    process.setWorkingDirectory(workDir);
    process.start(program, args);
    
    if (!process.waitForStarted(1000)) {
        result.error = "Failed to start process";
        result.exitCode = -1;
        return result;
    }
    
    // Send input
    process.write(test.input.toUtf8());
    process.closeWriteChannel();
    
    // Wait for completion with timeout
    if (!process.waitForFinished(timeoutMs)) {
        // Kill the entire process group to avoid orphans
#ifdef Q_OS_UNIX
        const qint64 childPid = process.processId();
        if (childPid > 0) {
            ::kill(-static_cast<pid_t>(childPid), SIGKILL);
        }
#endif
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
    if (result.exitCode == 0) {
        result.passed = (CompilationUtils::normalizeText(result.output) == CompilationUtils::normalizeText(test.expectedOutput));
    }
    
    return result;
}

void ParallelExecutor::cancel() {
    cancelled_ = true;
    if (watcher_ && watcher_->isRunning()) {
        watcher_->cancel();
        // Don't block the GUI thread; let the watcher's finished signal clean up
    }
    running_ = false;
}
