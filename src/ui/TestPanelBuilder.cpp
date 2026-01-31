#include "ui/TestPanelBuilder.h"
#include "ui/AutoResizingTextEdit.h"
#include "ui/IconUtils.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QVBoxLayout>

TestPanelBuilder::PanelWidgets TestPanelBuilder::build(QWidget *parent,
                                                       QObject * /*context*/,
                                                       const QColor &iconColor) {
    PanelWidgets widgets;
    editors_.clear();
    iconColor_ = iconColor;

    // Main panel
    QWidget *panel = new QWidget(parent);
    panel->setObjectName("DockContent");
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 12, 0, 0);
    layout->setSpacing(12);
    layout->setSizeConstraint(QLayout::SetNoConstraint);

    auto *titleRow = new QWidget(panel);
    auto *titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(12, 0, 12, 0);
    titleLayout->setSpacing(0);
    auto *titleLabel = new QLabel("Test cases", titleRow);
    titleLabel->setObjectName("PanelTitle");
    titleLayout->addWidget(titleLabel);
    layout->addWidget(titleRow);

    auto *metaRow = new QWidget(panel);
    auto *metaLayout = new QHBoxLayout(metaRow);
    metaLayout->setContentsMargins(12, 0, 12, 0);
    metaLayout->setSpacing(0);
    widgets.metaLabel = new QLabel(metaRow);
    widgets.metaLabel->setObjectName("TestMetaLabel");
    widgets.metaLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    widgets.metaLabel->setText("TL -  ML -");
    metaLayout->addWidget(widgets.metaLabel);
    layout->addWidget(metaRow);

    // Scrollable test cases area
    QScrollArea *scrollArea = new QScrollArea(panel);
    scrollArea->setObjectName("CasesScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    scrollArea->setMinimumSize(0, 0);
    scrollArea->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scrollArea->setAutoFillBackground(false);
    scrollArea->viewport()->setAutoFillBackground(false);

    widgets.summaryLabel = new QLabel(panel);
    widgets.summaryLabel->setObjectName("TestSummaryLabel");
    widgets.summaryLabel->setVisible(false);
    widgets.summaryLabel->setContentsMargins(12, 0, 12, 0);
    layout->addWidget(widgets.summaryLabel);

    // Cases container
    widgets.casesContainer = new QWidget(scrollArea);
    widgets.casesContainer->setObjectName("CasesContainer");
    widgets.casesContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    widgets.casesContainer->setMinimumSize(0, 0);
    widgets.casesContainer->setAutoFillBackground(false);
    
    widgets.casesLayout = new QVBoxLayout(widgets.casesContainer);
    widgets.casesLayout->setContentsMargins(12, 0, 12, 0);
    widgets.casesLayout->setSpacing(12);
    widgets.casesLayout->setAlignment(Qt::AlignTop);

    // Add button inside the scroll container, below the cases list
    widgets.addButton = new QPushButton(widgets.casesContainer);
    widgets.addButton->setObjectName("AddCaseButton");
    widgets.addButton->setToolTip("Add test case");
    widgets.addButton->setIcon(
        IconUtils::makeTintedIcon(":/images/plus.svg", iconColor_, QSize(16, 16)));
    widgets.addButton->setIconSize(QSize(16, 16));
    widgets.addButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widgets.addButton->setMinimumHeight(28);
    widgets.addButton->setFocusPolicy(Qt::NoFocus);
    widgets.casesLayout->addWidget(widgets.addButton);
    
    scrollArea->setWidget(widgets.casesContainer);
    layout->addWidget(scrollArea, 1);

    // Bottom action buttons row: Run All (left) and Delete All (right)
    QWidget *bottomRow = new QWidget(panel);
    bottomRow->setObjectName("CasesActionRow");
    bottomRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bottomRow->setFixedHeight(28);

    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    widgets.runAllButton = new QPushButton(bottomRow);
    widgets.runAllButton->setObjectName("RunAllButton");
    widgets.runAllButton->setToolTip("Run all test cases");
    widgets.runAllButton->setIcon(
        IconUtils::makeTintedIcon(":/images/play.svg", iconColor_, QSize(16, 16)));
    widgets.runAllButton->setIconSize(QSize(16, 16));
    widgets.runAllButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widgets.runAllButton->setMinimumHeight(28);
    widgets.runAllButton->setFocusPolicy(Qt::NoFocus);
    bottomLayout->addWidget(widgets.runAllButton, 1);

    widgets.clearCasesButton = new QPushButton(bottomRow);
    widgets.clearCasesButton->setObjectName("ClearCasesButton");
    widgets.clearCasesButton->setToolTip("Delete all test cases");
    widgets.clearCasesButton->setIcon(
        IconUtils::makeTintedIcon(":/images/trash.svg", iconColor_, QSize(16, 16)));
    widgets.clearCasesButton->setIconSize(QSize(16, 16));
    widgets.clearCasesButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widgets.clearCasesButton->setMinimumHeight(28);
    widgets.clearCasesButton->setFocusPolicy(Qt::NoFocus);
    bottomLayout->addWidget(widgets.clearCasesButton, 1);

    layout->addWidget(bottomRow);

    widgets.panel = panel;
    return widgets;
}

TestPanelBuilder::CaseWidgets TestPanelBuilder::createCase(QWidget *parent,
                                                           QObject *context,
                                                           int index) {
    CaseWidgets widgets;

    // Case panel
    QWidget *casePanel = new QWidget(parent);
    casePanel->setObjectName("TestCasePanel");
    casePanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    
    QVBoxLayout *layout = new QVBoxLayout(casePanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Header row with title and buttons
    QWidget *headerRow = new QWidget(casePanel);
    headerRow->setObjectName("TestCaseHeader");
    headerRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    QHBoxLayout *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    widgets.titleLabel = new QLabel(QString("TC %1").arg(index), headerRow);
    widgets.titleLabel->setObjectName("TestCaseTitle");
    headerLayout->addWidget(widgets.titleLabel);

    widgets.statusLabel = new QLabel("-", headerRow);
    widgets.statusLabel->setObjectName("RunStatus");
    headerLayout->addWidget(widgets.statusLabel);

    headerLayout->addStretch();

    // Action buttons in header row
    QWidget *actionButtons = new QWidget(headerRow);
    QHBoxLayout *actionLayout = new QHBoxLayout(actionButtons);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(6);

    widgets.runButton = new QPushButton(actionButtons);
    widgets.runButton->setObjectName("RunButton");
    widgets.runButton->setToolTip("Compile and Run");
    widgets.runButton->setIcon(
        IconUtils::makeTintedIcon(":/images/play.svg", iconColor_, QSize(16, 16)));
    widgets.runButton->setIconSize(QSize(16, 16));
    widgets.runButton->setFixedSize(28, 28);
    widgets.runButton->setFocusPolicy(Qt::NoFocus);
    actionLayout->addWidget(widgets.runButton);

    widgets.deleteButton = new QPushButton(actionButtons);
    widgets.deleteButton->setObjectName("DeleteButton");
    widgets.deleteButton->setToolTip("Delete test case");
    widgets.deleteButton->setIcon(
        IconUtils::makeTintedIcon(":/images/trash.svg", iconColor_, QSize(16, 16)));
    widgets.deleteButton->setIconSize(QSize(16, 16));
    widgets.deleteButton->setFixedSize(28, 28);
    widgets.deleteButton->setFocusPolicy(Qt::NoFocus);
    actionLayout->addWidget(widgets.deleteButton);

    headerLayout->addStretch();
    headerLayout->addWidget(actionButtons);
    layout->addWidget(headerRow);

    // Input and expected output blocks
    QWidget *inputBlock = createLabeledBlock(
        casePanel, "Input", widgets.inputEditor, "InputBox",
        "Enter input...", false, 1, 8);
    
    QWidget *expectedBlock = createLabeledBlock(
        casePanel, "Expected Output", widgets.expectedEditor, "ExpectedBox",
        "Enter expected output...", false, 1, 8);

    layout->addWidget(inputBlock);
    layout->addWidget(expectedBlock);

    // Setup placeholder behavior for input/expected editors
    auto setupPlaceholder = [context](AutoResizingTextEdit *editor, const QString &placeholder) {
        if (!editor) {
            return;
        }
        QPointer<AutoResizingTextEdit> safeEditor(editor);
        editor->setElidedPlaceholderText(placeholder);
        auto updatePlaceholder = [safeEditor]() {
            if (!safeEditor) {
                return;
            }
            if (safeEditor->hasFocus()) {
                safeEditor->setPlaceholderVisible(false);
            } else if (safeEditor->toPlainText().isEmpty()) {
                safeEditor->setPlaceholderVisible(true);
            }
        };
        QObject::connect(editor, &AutoResizingTextEdit::textChanged, context, updatePlaceholder);
        QObject::connect(qApp, &QApplication::focusChanged, context,
                         [updatePlaceholder](QWidget*, QWidget*) { updatePlaceholder(); });
        updatePlaceholder();
    };

    setupPlaceholder(widgets.inputEditor, "Enter input...");
    setupPlaceholder(widgets.expectedEditor, "Enter expected output...");

    // Output splitter (initially hidden)
    widgets.outputSplitter = new QSplitter(Qt::Vertical, casePanel);
    widgets.outputSplitter->setObjectName("OutputSplitter");
    widgets.outputSplitter->setChildrenCollapsible(false);
    widgets.outputSplitter->setHandleWidth(6);
    widgets.outputSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    widgets.outputBlock = createLabeledBlock(
        widgets.outputSplitter, "Your Output", widgets.outputViewer, "OutputBox",
        QString(), true, 1, 8);
    
    widgets.errorBlock = createLabeledBlock(
        widgets.outputSplitter, "Debug (stderr)", widgets.errorViewer, "ErrorBox",
        QString(), true, 1, 8);

    widgets.outputSplitter->addWidget(widgets.outputBlock);
    widgets.outputSplitter->addWidget(widgets.errorBlock);
    widgets.outputSplitter->setStretchFactor(0, 1);
    widgets.outputSplitter->setStretchFactor(1, 1);
    
    layout->addWidget(widgets.outputSplitter);

    // Initially hide output sections
    widgets.outputBlock->setVisible(false);
    widgets.errorBlock->setVisible(false);
    widgets.outputSplitter->setVisible(false);

    widgets.panel = casePanel;
    return widgets;
}

void TestPanelBuilder::refreshEditorSizing() {
    // Remove null pointers and refresh remaining editors
    editors_.erase(
        std::remove_if(editors_.begin(), editors_.end(),
                       [](const QPointer<AutoResizingTextEdit> &e) { return e.isNull(); }),
        editors_.end());

    for (auto &editor : editors_) {
        if (editor) {
            editor->refreshHeight();
        }
    }
}

QWidget *TestPanelBuilder::createLabeledBlock(QWidget *parent,
                                              const QString &title,
                                              AutoResizingTextEdit *&editor,
                                              const QString &objectName,
                                              const QString &placeholder,
                                              bool readOnly,
                                              int minLines,
                                              int maxLines) {
    QWidget *block = new QWidget(parent);
    block->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    
    QVBoxLayout *layout = new QVBoxLayout(block);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    QLabel *label = new QLabel(title, block);
    layout->addWidget(label);

    editor = new AutoResizingTextEdit(block);
    editor->setObjectName(objectName);
    editor->setReadOnly(readOnly);
    editor->setLineRange(minLines, maxLines);
    
    if (!placeholder.isEmpty()) {
        editor->setElidedPlaceholderText(placeholder);
    }

    layout->addWidget(editor);

    // Track for refreshEditorSizing
    editors_.push_back(editor);

    return block;
}
