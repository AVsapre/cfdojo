#include "execution/ParallelExecutor.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QList>
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
    if (template_.contains("//#main")) {
        QString result = template_;
        return result.replace("//#main", solution);
    }
    return solution;
}

void ParallelExecutor::runAll(const std::vector<TestInput> &tests) {
    if (running_) {
        return;
    }
    
    running_ = true;
    cancelled_ = false;
    
    emit compilationStarted();
    
    // Compile first (on main thread to set up temp dir)
    if (!compile()) {
        running_ = false;
        return;
    }
    
    emit compilationFinished(true, QString());
    
    // Run tests in parallel using QtConcurrent
    QList<TestInput> testList(tests.begin(), tests.end());
    expectedResults_ = static_cast<int>(tests.size());
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
}

bool ParallelExecutor::compile() {
    // Create temp directory
    tempDir_ = std::make_unique<QTemporaryDir>();
    if (!tempDir_->isValid()) {
        emit compilationFinished(false, "Failed to create temporary directory");
        return false;
    }
    
    const QString tempPath = tempDir_->path();
    const QString sourcePath = QDir(tempPath).filePath("solution.cpp");
    executablePath_ = QDir(tempPath).filePath("solution");
    
    // Write source code with transclusion
    const QString code = applyTransclusion(sourceCode_);
    QFile file(sourcePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit compilationFinished(false, "Failed to write source file");
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << code;
    file.close();
    
    // Compile
    QProcess compiler;
    compiler.setWorkingDirectory(tempPath);
    
    QStringList args;
    if (!compilerFlags_.isEmpty()) {
        args = compilerFlags_.split(' ', Qt::SkipEmptyParts);
    }
    args << "-o" << executablePath_ << sourcePath;
    
    compiler.start(compilerPath_, args);
    if (!compiler.waitForFinished(30000)) {
        emit compilationFinished(false, "Compilation timed out");
        return false;
    }
    
    if (compiler.exitCode() != 0) {
        const QString error = QString::fromUtf8(compiler.readAllStandardError());
        emit compilationFinished(false, error);
        return false;
    }
    
    return true;
}

TestResult ParallelExecutor::runSingleTest(const TestInput &test) {
    TestResult result;
    result.testIndex = test.testIndex;
    
    QElapsedTimer timer;
    timer.start();
    
    QProcess process;
    process.start(executablePath_, {});
    
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
