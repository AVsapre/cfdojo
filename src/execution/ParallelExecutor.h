#pragma once

#include "execution/CompilationConfig.h"

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QProcess>
#include <QTemporaryDir>
#include <QStringList>
#include <atomic>
#include <memory>
#include <vector>

class QsciScintilla;

// Result of a single test case execution
struct TestResult {
    int testIndex = -1;
    QString output;
    QString error;
    int exitCode = -1;
    bool passed = false;
    qint64 executionTimeMs = 0;
};

// Input for a single test case
struct TestInput {
    int testIndex = -1;
    QString input;
    QString expectedOutput;
};

class ParallelExecutor : public QObject {
    Q_OBJECT

public:
    explicit ParallelExecutor(QObject *parent = nullptr);
    ~ParallelExecutor();

    // Set the source code
    void setSourceCode(const QString &code);

    // Set all compilation settings at once
    void setConfig(const CompilationConfig &cfg) { config_ = cfg; }
    const CompilationConfig &config() const { return config_; }

    void setTimeout(int ms) { timeoutMs_ = ms; }

    // Run all tests in parallel (compiles once, then runs tests concurrently)
    void runAll(const std::vector<TestInput> &tests);
    
    // Check if currently running
    bool isRunning() const { return running_; }
    
    // Cancel execution
    void cancel();

signals:
    void compilationStarted();
    void compilationFinished(bool success, const QString &error);
    void testFinished(const TestResult &result);
    void allTestsFinished(const std::vector<TestResult> &results);

private:
    bool compile();
    TestResult runSingleTest(const TestInput &test,
                             const QString &program,
                             const QStringList &args,
                             const QString &workDir,
                             int timeoutMs);

    CompilationConfig config_;
    QString sourceCode_;
    int timeoutMs_ = 5000;
    
    std::unique_ptr<QTemporaryDir> tempDir_;
    QString executablePath_;
    QString runProgram_;
    QStringList runArgs_;
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};

    QFutureWatcher<TestResult> *watcher_ = nullptr;
    QFuture<void> compileFuture_;
    std::vector<TestResult> results_;
    int expectedResults_ = 0;
};
