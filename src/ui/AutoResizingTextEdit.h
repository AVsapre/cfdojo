#pragma once

#include <QPlainTextEdit>
#include <QSize>
#include <QString>

class AutoResizingTextEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit AutoResizingTextEdit(QWidget *parent = nullptr);

    void setElidedPlaceholderText(const QString &text);
    void setPlaceholderVisible(bool visible);

    void setLineRange(int minLines, int maxLines);
    int minLines() const { return minLines_; }
    int maxLines() const { return maxLines_; }

    void refreshHeight();

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private slots:
    void adjustHeight();

private:
    int calculateHeight(int lines) const;
    int targetLineCount() const;
    void updateElidedPlaceholder();
    QString elideWithDots(const QString &text, int maxWidth) const;

    int minLines_ = 1;
    int maxLines_ = 0;  // 0 means no auto-resize
    QString fullPlaceholder_;
    bool placeholderVisible_ = true;
};
