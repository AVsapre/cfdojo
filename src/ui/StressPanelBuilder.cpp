#include "ui/StressPanelBuilder.h"

#include "ui/IconUtils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QIntValidator>

StressPanelBuilder::Widgets StressPanelBuilder::build(QWidget *parent,
                                                      const QColor &iconColor) const {
    Widgets widgets;

    QWidget *panel = new QWidget(parent);
    panel->setObjectName("StressTestPanel");
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 12, 0, 0);
    layout->setSpacing(12);

    auto *titleRow = new QWidget(panel);
    auto *titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(12, 0, 12, 0);
    titleLayout->setSpacing(0);
    auto *titleLabel = new QLabel("Stress testing", titleRow);
    titleLabel->setObjectName("PanelTitle");
    titleLayout->addWidget(titleLabel);
    layout->addWidget(titleRow);

    auto *header = new QWidget(panel);
    header->setObjectName("StressTestHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    headerLayout->setSpacing(8);

    auto *statusLabel = new QLabel("Idle", header);
    statusLabel->setObjectName("StressTestStatus");
    headerLayout->addWidget(statusLabel);

    headerLayout->addStretch();

    auto *runButton = new QPushButton(header);
    runButton->setObjectName("RunButton");
    runButton->setToolTip("Run stress test");
    runButton->setIcon(
        IconUtils::makeTintedIcon(":/images/play.svg", iconColor, QSize(16, 16)));
    runButton->setIconSize(QSize(16, 16));
    runButton->setFixedSize(28, 28);
    runButton->setFocusPolicy(Qt::NoFocus);
    headerLayout->addWidget(runButton);

    layout->addWidget(header);

    auto *controls = new QWidget(panel);
    auto *controlsLayout = new QHBoxLayout(controls);
    controlsLayout->setContentsMargins(12, 0, 12, 0);
    controlsLayout->setSpacing(8);

    auto *generateLabel = new QLabel("Generate", controls);
    generateLabel->setObjectName("StressGenerateLabel");
    controlsLayout->addWidget(generateLabel);

    auto *countEdit = new QLineEdit(controls);
    countEdit->setObjectName("StressGenerateCount");
    countEdit->setText("100");
    countEdit->setFixedWidth(48);
    countEdit->setAlignment(Qt::AlignRight);
    countEdit->setValidator(new QIntValidator(1, 10000, countEdit));
    QObject::connect(countEdit, &QLineEdit::editingFinished, countEdit, [countEdit]() {
        if (countEdit->text().trimmed().isEmpty()) {
            countEdit->setText("1");
        }
    });
    controlsLayout->addWidget(countEdit);

    auto *casesLabel = new QLabel("testcases", controls);
    casesLabel->setObjectName("StressGenerateSuffix");
    controlsLayout->addWidget(casesLabel);
    controlsLayout->addStretch();

    layout->addWidget(controls);

    auto *log = new QPlainTextEdit(panel);
    log->setObjectName("StressLog");
    log->setReadOnly(true);
    log->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(log);

    widgets.panel = panel;
    widgets.statusLabel = statusLabel;
    widgets.runButton = runButton;
    widgets.countEdit = countEdit;
    widgets.log = log;
    return widgets;
}
