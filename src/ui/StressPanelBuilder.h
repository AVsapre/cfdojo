#pragma once

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;
class QWidget;

class StressPanelBuilder {
public:
    struct Widgets {
        QWidget *panel = nullptr;
        QLabel *statusLabel = nullptr;
        QPushButton *runButton = nullptr;
        QLineEdit *countEdit = nullptr;
        QPlainTextEdit *log = nullptr;
    };

    Widgets build(QWidget *parent, const QColor &iconColor) const;
};
class QColor;
