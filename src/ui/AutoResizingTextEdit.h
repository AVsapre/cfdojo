#pragma once

#include <QPlainTextEdit>

class AutoResizingTextEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit AutoResizingTextEdit(QWidget *parent = nullptr);

    void setLineRange(int minLines, int maxLines);
    int minLines() const { return minLines_; }
    int maxLines() const { return maxLines_; }

    void refreshHeight();

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void adjustHeight();

private:
    int calculateHeight(int lines) const;

    int minLines_ = 1;
    int maxLines_ = 0;  // 0 means no auto-resize
};
