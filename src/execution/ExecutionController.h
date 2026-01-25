#pragma once

#include <QObject>
#include <QProcess>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QsciScintilla;
class QWidget;

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
    
    void bind(const UiBindings &bindings);
    void runWithBindings(const UiBindings &bindings);
    
    State state() const { return state_; }

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
    void startCompilation();
    void startExecution();
    void updateStatus(const QString &status);
    void updateOutputPanels(bool showOutput, bool showError);
    void clearOutputs();
    QString normalizeText(const QString &text) const;

    UiBindings ui_;
    State state_ = State::Idle;
    QProcess *compilerProcess_;
    QProcess *runProcess_;
};
