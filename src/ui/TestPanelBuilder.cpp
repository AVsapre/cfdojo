#include "ui/TestPanelBuilder.h"
#include "ui/AutoResizingTextEdit.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

// Scroll area with zero minimum size to allow proper collapsing
class FlexibleScrollArea : public QScrollArea {
public:
    explicit FlexibleScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {}
    QSize minimumSizeHint() const override { return QSize(0, 0); }
};

} // namespace

TestPanelBuilder::PanelWidgets TestPanelBuilder::build(QWidget *parent, QObject * /*context*/) {
    PanelWidgets widgets;
    editors_.clear();

    // Main panel
    QWidget *panel = new QWidget(parent);
    panel->setObjectName("DockContent");
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->setSizeConstraint(QLayout::SetNoConstraint);

    // Header spacer
    QWidget *header = new QWidget(panel);
    header->setObjectName("TestPanelHeader");
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    header->setFixedHeight(4);
    layout->addWidget(header);

    // Scrollable test cases area
    FlexibleScrollArea *scrollArea = new FlexibleScrollArea(panel);
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
    
    scrollArea->setWidget(widgets.casesContainer);
    layout->addWidget(scrollArea, 1);

    // Action buttons row
    QWidget *actionsRow = new QWidget(panel);
    actionsRow->setObjectName("CasesActionRow");
    actionsRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    actionsRow->setFixedHeight(28);
    
    QHBoxLayout *actionsLayout = new QHBoxLayout(actionsRow);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(0);

    // Add button
    widgets.addButton = new QPushButton(actionsRow);
    widgets.addButton->setObjectName("AddCaseButton");
    widgets.addButton->setToolTip("Add test case");
    widgets.addButton->setIcon(QIcon(":/images/plus.svg"));
    widgets.addButton->setIconSize(QSize(16, 16));
    widgets.addButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widgets.addButton->setMinimumHeight(28);
    widgets.addButton->setFocusPolicy(Qt::NoFocus);
    actionsLayout->addWidget(widgets.addButton, 1);

    // Clear button
    widgets.clearCasesButton = new QPushButton(actionsRow);
    widgets.clearCasesButton->setObjectName("ClearCasesButton");
    widgets.clearCasesButton->setToolTip("Delete all test cases");
    widgets.clearCasesButton->setIcon(QIcon(":/images/trash.svg"));
    widgets.clearCasesButton->setIconSize(QSize(16, 16));
    widgets.clearCasesButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    widgets.clearCasesButton->setMinimumHeight(28);
    widgets.clearCasesButton->setFocusPolicy(Qt::NoFocus);
    actionsLayout->addWidget(widgets.clearCasesButton, 1);

    layout->addWidget(actionsRow);

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

    widgets.titleLabel = new QLabel(QString("Test Case %1").arg(index), headerRow);
    widgets.titleLabel->setObjectName("TestCaseTitle");
    headerLayout->addWidget(widgets.titleLabel);
    headerLayout->addStretch();

    // Action buttons
    QWidget *actionButtons = new QWidget(headerRow);
    QHBoxLayout *actionLayout = new QHBoxLayout(actionButtons);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(6);

    widgets.runButton = new QPushButton(actionButtons);
    widgets.runButton->setObjectName("RunButton");
    widgets.runButton->setToolTip("Compile and Run");
    widgets.runButton->setIcon(QIcon(":/images/play.svg"));
    widgets.runButton->setIconSize(QSize(18, 18));
    widgets.runButton->setFixedSize(30, 26);
    widgets.runButton->setFocusPolicy(Qt::NoFocus);
    actionLayout->addWidget(widgets.runButton);

    widgets.deleteButton = new QPushButton(actionButtons);
    widgets.deleteButton->setObjectName("DeleteButton");
    widgets.deleteButton->setToolTip("Delete test case");
    widgets.deleteButton->setIcon(QIcon(":/images/trash.svg"));
    widgets.deleteButton->setIconSize(QSize(16, 16));
    widgets.deleteButton->setFixedSize(30, 26);
    widgets.deleteButton->setFocusPolicy(Qt::NoFocus);
    actionLayout->addWidget(widgets.deleteButton);

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
        auto updatePlaceholder = [editor, placeholder]() {
            if (editor->hasFocus()) {
                editor->setPlaceholderText(QString());
            } else if (editor->toPlainText().isEmpty()) {
                editor->setPlaceholderText(placeholder);
            }
        };
        QObject::connect(editor, &AutoResizingTextEdit::textChanged, context, updatePlaceholder);
        QObject::connect(qApp, &QApplication::focusChanged, context, 
                         [updatePlaceholder](QWidget*, QWidget*) { updatePlaceholder(); });
        updatePlaceholder();
    };

    setupPlaceholder(widgets.inputEditor, "Enter input...");
    setupPlaceholder(widgets.expectedEditor, "Enter expected output...");

    // Status label
    widgets.statusLabel = new QLabel("Status: -", casePanel);
    widgets.statusLabel->setObjectName("RunStatus");
    layout->addWidget(widgets.statusLabel);

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
        editor->setPlaceholderText(placeholder);
    }

    layout->addWidget(editor);

    // Track for refreshEditorSizing
    editors_.push_back(editor);

    return block;
}
