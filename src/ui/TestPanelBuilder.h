#pragma once

#include <QString>
#include <QPointer>
#include <QColor>
#include <vector>

class QLabel;
class QPushButton;
class QSplitter;
class QWidget;
class QObject;
class QVBoxLayout;
class AutoResizingTextEdit;

class TestPanelBuilder {
public:
    struct CaseWidgets {
        QWidget *panel = nullptr;
        QLabel *titleLabel = nullptr;
        AutoResizingTextEdit *inputEditor = nullptr;
        AutoResizingTextEdit *expectedEditor = nullptr;
        AutoResizingTextEdit *outputViewer = nullptr;
        AutoResizingTextEdit *errorViewer = nullptr;
        QLabel *statusLabel = nullptr;
        QPushButton *runButton = nullptr;
        QPushButton *deleteButton = nullptr;
        QSplitter *outputSplitter = nullptr;
        QWidget *outputBlock = nullptr;
        QWidget *errorBlock = nullptr;
    };

    struct PanelWidgets {
        QWidget *panel = nullptr;
        QWidget *casesContainer = nullptr;
        QVBoxLayout *casesLayout = nullptr;
        QLabel *summaryLabel = nullptr;
        QPushButton *runAllButton = nullptr;
        QPushButton *addButton = nullptr;
        QPushButton *clearCasesButton = nullptr;
    };

    PanelWidgets build(QWidget *parent, QObject *context, const QColor &iconColor);
    CaseWidgets createCase(QWidget *parent, QObject *context, int index);
    void refreshEditorSizing();

private:
    QWidget *createLabeledBlock(QWidget *parent,
                                const QString &title,
                                AutoResizingTextEdit *&editor,
                                const QString &objectName,
                                const QString &placeholder,
                                bool readOnly,
                                int minLines,
                                int maxLines);

    std::vector<QPointer<AutoResizingTextEdit>> editors_;
    QColor iconColor_ = QColor("#d4d4d4");
};
