#include "ui/AutoResizingTextEdit.h"

#include <QFontMetrics>
#include <QResizeEvent>
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

void AutoResizingTextEdit::setElidedPlaceholderText(const QString &text) {
    fullPlaceholder_ = text;
    if (placeholderVisible_) {
        updateElidedPlaceholder();
    }
}

void AutoResizingTextEdit::setPlaceholderVisible(bool visible) {
    placeholderVisible_ = visible;
    if (visible) {
        updateElidedPlaceholder();
    } else {
        QPlainTextEdit::setPlaceholderText(QString());
    }
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
        if (placeholderVisible_) {
            updateElidedPlaceholder();
        }
    }
}

void AutoResizingTextEdit::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    if (placeholderVisible_) {
        updateElidedPlaceholder();
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

void AutoResizingTextEdit::updateElidedPlaceholder() {
    if (fullPlaceholder_.isEmpty()) {
        QPlainTextEdit::setPlaceholderText(QString());
        return;
    }

    const int frame = frameWidth();
    const int docMargin = static_cast<int>(std::ceil(document()->documentMargin()));
    const int padding = (frame + docMargin) * 2;
    const int safetyPadding = std::max(1, QFontMetrics(font()).horizontalAdvance('.'));
    const int available = std::max(0, viewport()->width() - padding - safetyPadding);

    const QString elided = elideWithDots(fullPlaceholder_, available);
    QPlainTextEdit::setPlaceholderText(elided);
}

QString AutoResizingTextEdit::elideWithDots(const QString &text, int maxWidth) const {
    if (text.isEmpty() || maxWidth <= 0) {
        return QString();
    }

    const QFontMetrics metrics(font());
    if (metrics.horizontalAdvance(text) <= maxWidth) {
        return text;
    }

    static const QString kEllipsis = "...";
    const int ellipsisWidth = metrics.horizontalAdvance(kEllipsis);
    const int targetWidth = std::max(0, maxWidth - ellipsisWidth);

    int low = 0;
    int high = text.size();
    while (low < high) {
        const int mid = (low + high + 1) / 2;
        if (metrics.horizontalAdvance(text.left(mid)) <= targetWidth) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }

    return text.left(low) + kEllipsis;
}
