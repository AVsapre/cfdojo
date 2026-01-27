#pragma once

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QProcess>
#include <QTemporaryDir>
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

    // Set the source code and template
    void setSourceCode(const QString &code);
    void setTemplate(const QString &tmpl);
    
    // Set compiler settings
    void setCompilerPath(const QString &path) { compilerPath_ = path; }
    void setCompilerFlags(const QString &flags) { compilerFlags_ = flags; }
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
    void testStarted(int testIndex);
    void testFinished(const TestResult &result);
    void allTestsFinished(const std::vector<TestResult> &results);

private:
    QString applyTransclusion(const QString &solution) const;
    bool compile();
    TestResult runSingleTest(const TestInput &test);
    QString normalizeText(const QString &text) const;

    QString sourceCode_;
    QString template_ = "//#main";
    QString compilerPath_ = "g++";
    QString compilerFlags_ = "-O2 -std=c++17";
    int timeoutMs_ = 5000;
    
    std::unique_ptr<QTemporaryDir> tempDir_;
    QString executablePath_;
    bool running_ = false;
    bool cancelled_ = false;

    QFutureWatcher<TestResult> *watcher_ = nullptr;
    std::vector<TestResult> results_;
    int expectedResults_ = 0;
};
