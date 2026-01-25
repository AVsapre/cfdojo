#include "ui/AutoResizingTextEdit.h"

#include <QFontMetrics>
#include <QTextDocument>
#include <algorithm>
#include <cmath>

AutoResizingTextEdit::AutoResizingTextEdit(QWidget *parent)
    : QPlainTextEdit(parent) {
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    connect(this, &QPlainTextEdit::textChanged, this, &AutoResizingTextEdit::adjustHeight);
}

void AutoResizingTextEdit::setLineRange(int minLines, int maxLines) {
    minLines_ = std::max(1, minLines);
    maxLines_ = maxLines;
    adjustHeight();
}

void AutoResizingTextEdit::refreshHeight() {
    adjustHeight();
}

void AutoResizingTextEdit::changeEvent(QEvent *event) {
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        adjustHeight();
    }
}

int AutoResizingTextEdit::calculateHeight(int lines) const {
    const int lineHeight = QFontMetrics(font()).lineSpacing();
    const int frame = frameWidth();
    const int docMargin = static_cast<int>(std::ceil(document()->documentMargin()));
    const int verticalPadding = (frame + docMargin) * 2;
    return (lineHeight * lines) + verticalPadding;
}

void AutoResizingTextEdit::adjustHeight() {
    const int minHeight = calculateHeight(minLines_);
    setMinimumHeight(minHeight);

    if (maxLines_ <= 0) {
        // No auto-resize, just set minimum
        setFixedHeight(minHeight);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        return;
    }

    const int clampedMaxLines = std::max(maxLines_, minLines_);
    const int docLines = document()->blockCount();
    const int clampedLines = std::clamp(docLines, minLines_, clampedMaxLines);
    
    setFixedHeight(calculateHeight(clampedLines));
    
    if (docLines > clampedMaxLines) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    
    updateGeometry();
}
