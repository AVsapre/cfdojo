#include "execution/ExecutionController.h"

#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QTextStream>
#include <Qsci/qsciscintilla.h>

ExecutionController::ExecutionController(QObject *parent)
    : QObject(parent),
      compilerProcess_(new QProcess(this)),
      runProcess_(new QProcess(this)) {
    
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

void ExecutionController::bind(const UiBindings &bindings) {
    ui_ = bindings;
}

void ExecutionController::runWithBindings(const UiBindings &bindings) {
    ui_ = bindings;
    
    if (state_ != State::Idle) {
        qDebug() << "Execution already in progress";
        return;
    }
    
    startCompilation();
}

void ExecutionController::setState(State newState) {
    if (state_ != newState) {
        state_ = newState;
        emit stateChanged(newState);
    }
}

void ExecutionController::startCompilation() {
    setState(State::Compiling);
    updateStatus("Compiling...");
    clearOutputs();

    // Write source code to file
    const QString code = ui_.codeEditor ? ui_.codeEditor->text() : QString();
    QFile file("solution.cpp");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << code;
        file.close();
    }

    qDebug() << "Compiling...";

    // Kill any running compilation
    if (compilerProcess_->state() != QProcess::NotRunning) {
        compilerProcess_->kill();
        compilerProcess_->waitForFinished(1000);
    }

    compilerProcess_->start("g++", {"solution.cpp", "-o", "solution"});
}

void ExecutionController::onCompilationFinished(int exitCode, QProcess::ExitStatus status) {
    if (exitCode == 0 && status == QProcess::NormalExit) {
        qDebug() << "Compilation succeeded";
        emit compilationSucceeded();
        startExecution();
    } else {
        const QString error = QString::fromUtf8(compilerProcess_->readAllStandardError());
        qDebug() << "Compilation failed:" << error;
        
        updateStatus("Compile Error");
        if (ui_.errorViewer) {
            ui_.errorViewer->setPlainText(error);
        }
        updateOutputPanels(false, !error.isEmpty());
        
        setState(State::Idle);
        emit compilationFailed(error);
    }
}

void ExecutionController::startExecution() {
    setState(State::Running);
    updateStatus("Running...");
    
    qDebug() << "Running ./solution...";

    // Kill any running execution
    if (runProcess_->state() != QProcess::NotRunning) {
        runProcess_->kill();
        runProcess_->waitForFinished(1000);
    }

    runProcess_->start("./solution");
}

void ExecutionController::onRunFinished(int exitCode, QProcess::ExitStatus status) {
    const QString stdOut = QString::fromUtf8(runProcess_->readAllStandardOutput());
    const QString stdErr = QString::fromUtf8(runProcess_->readAllStandardError());

    // Update output viewers
    if (ui_.outputViewer) {
        ui_.outputViewer->setPlainText(stdOut);
    }
    if (ui_.errorViewer) {
        ui_.errorViewer->setPlainText(stdErr);
    }
    updateOutputPanels(!stdOut.isEmpty(), !stdErr.isEmpty());

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
    
    updateStatus(resultStatus);
    setState(State::Idle);
    emit executionFinished(stdOut, stdErr, exitCode);
}

void ExecutionController::onRunError(QProcess::ProcessError /*error*/) {
    updateStatus("Run Failed");
    
    if (ui_.errorViewer) {
        const QString errorText = runProcess_->errorString();
        ui_.errorViewer->setPlainText(errorText);
        updateOutputPanels(false, !errorText.isEmpty());
    }
    
    setState(State::Idle);
}

void ExecutionController::updateStatus(const QString &status) {
    if (ui_.statusLabel) {
        ui_.statusLabel->setText(QString("Status: %1").arg(status));
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

QString ExecutionController::normalizeText(const QString &text) const {
    QString result = text;
    result.replace("\r\n", "\n");
    
    // Remove trailing whitespace
    while (!result.isEmpty() && result.back().isSpace()) {
        result.chop(1);
    }
    
    return result;
}
