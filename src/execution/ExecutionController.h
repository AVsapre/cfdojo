#pragma once

#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QColor>
#include <memory>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QsciScintilla;
class QWidget;
class QTimer;

class ExecutionController : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Compiling,
        Running,
        Finished
    };
    Q_ENUM(State)

    struct UiBindings {
        QsciScintilla *codeEditor = nullptr;
        QPlainTextEdit *inputEditor = nullptr;
        QPlainTextEdit *expectedEditor = nullptr;
        QPlainTextEdit *outputViewer = nullptr;
        QPlainTextEdit *errorViewer = nullptr;
        QLabel *statusLabel = nullptr;
        QSplitter *outputSplitter = nullptr;
        QWidget *outputBlock = nullptr;
        QWidget *errorBlock = nullptr;
        QPushButton *runButton = nullptr;
    };

    explicit ExecutionController(QObject *parent = nullptr);
    ~ExecutionController();
    
    void bind(const UiBindings &bindings);
    void runWithBindings(const UiBindings &bindings);
    void stop();
    void setIconTintColor(const QColor &color);
    
    // Set template for transclusion (//#main is replaced with solution code)
    void setTemplate(const QString &tmpl) { template_ = tmpl; }
    QString getTemplate() const { return template_; }
    void setTimeoutMs(int ms) { timeoutMs_ = ms; }
    
    // Apply transclusion: replace //#main with solution code
    QString applyTransclusion(const QString &solution) const;
    
    State state() const { return state_; }
    qint64 lastExecutionTimeMs() const { return lastExecutionTimeMs_; }

signals:
    void stateChanged(State newState);
    void compilationSucceeded();
    void compilationFailed(const QString &error);
    void executionFinished(const QString &output, const QString &error, int exitCode);

private slots:
    void onCompilationFinished(int exitCode, QProcess::ExitStatus status);
    void onRunFinished(int exitCode, QProcess::ExitStatus status);
    void onRunError(QProcess::ProcessError error);

private:
    void setState(State newState);
    void updateRunButtonForState(State newState);
    void startCompilation();
    void startExecution();
    void cleanupTempDir();
    void updateStatus(const QString &status);
    void updateOutputPanels(bool showOutput, bool showError);
    void clearOutputs();
    QString normalizeText(const QString &text) const;

    UiBindings ui_;
    State state_ = State::Idle;
    QString template_ = "//#main";  // Default template (just transcludes solution)
    QProcess *compilerProcess_;
    QProcess *runProcess_;
    std::unique_ptr<QTemporaryDir> tempDir_;
    bool stopRequested_ = false;
    QElapsedTimer runTimer_;
    qint64 lastExecutionTimeMs_ = -1;
    QTimer *timeoutTimer_ = nullptr;
    int timeoutMs_ = 5000;
    bool timedOut_ = false;
    QColor iconColor_ = QColor("#d4d4d4");
};
