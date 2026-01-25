#pragma once

#include <QLabel>

class QsciScintilla;

// A floating placeholder label that appears in QsciScintilla when empty and unfocused
class EditorPlaceholder : public QLabel {
    Q_OBJECT

public:
    explicit EditorPlaceholder(QsciScintilla *editor, const QString &text);

public slots:
    void updatePosition();
    void updateVisibility();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QsciScintilla *editor_;
};
