#pragma once

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;
class QWidget;
class QColor;

class StressPanelBuilder {
public:
    struct Widgets {
        QWidget *panel = nullptr;
        QLabel *statusLabel = nullptr;
        QLabel *complexityLabel = nullptr;
        QPushButton *runButton = nullptr;
        QLineEdit *countEdit = nullptr;
        QPlainTextEdit *log = nullptr;
    };

    Widgets build(QWidget *parent, const QColor &iconColor) const;
};
