#pragma once

#include <QFont>
#include <QLabel>

class QsciScintilla;

// A floating placeholder label that appears in QsciScintilla when empty and unfocused
class EditorPlaceholder : public QLabel {
    Q_OBJECT

public:
    explicit EditorPlaceholder(QsciScintilla *editor, const QString &text);
    
    void setBaseFont(const QFont &font);

public slots:
    void updatePosition();
    void updateVisibility();
    void updateZoom();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateElidedText();
    void applyZoomedFont();
    QString elideWithDots(const QString &text, int maxWidth) const;

    QsciScintilla *editor_;
    QString fullText_;
    QFont baseFont_;
    int textX_ = 0;
    int lastZoom_ = 0;
};
