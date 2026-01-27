#include "editor/EditorPlaceholder.h"

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QTimer>
#include <Qsci/qsciscintilla.h>
#include <algorithm>

EditorPlaceholder::EditorPlaceholder(QsciScintilla *editor, const QString &text)
    : QLabel(text, editor), editor_(editor), fullText_(text) {
    setObjectName("EditorPlaceholder");
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setContentsMargins(0, 0, 0, 0);
    
    // Install event filter on editor to track relevant changes
    editor_->installEventFilter(this);
    
    // Connect to editor signals
    connect(editor_, &QsciScintilla::textChanged, this, &EditorPlaceholder::updateVisibility);
    connect(editor_, &QsciScintilla::textChanged, this, &EditorPlaceholder::updatePosition);
    connect(editor_, &QsciScintilla::cursorPositionChanged, this, &EditorPlaceholder::updatePosition);
    connect(editor_, &QsciScintilla::linesChanged, this, &EditorPlaceholder::updatePosition);
    
    // Track focus changes
    connect(qApp, &QApplication::focusChanged, this, [this]() {
        updateVisibility();
    });
    
    // Initialize zoom tracking
    lastZoom_ = static_cast<int>(editor_->SendScintilla(QsciScintilla::SCI_GETZOOM));
    
    updateElidedText();
    updatePosition();
    updateVisibility();
    raise();
}

void EditorPlaceholder::setBaseFont(const QFont &font) {
    baseFont_ = font;
    setFont(font);
    updateElidedText();
    updatePosition();
}

void EditorPlaceholder::updatePosition() {
    if (!editor_) {
        return;
    }

    const long pos = 0;

    const int textX = static_cast<int>(editor_->SendScintilla(
        QsciScintilla::SCI_POINTXFROMPOSITION,
        static_cast<unsigned long>(0),
        static_cast<long>(pos)));

    const int textY = static_cast<int>(editor_->SendScintilla(
        QsciScintilla::SCI_POINTYFROMPOSITION,
        static_cast<unsigned long>(0),
        static_cast<long>(pos)));

    textX_ = textX;
    move(textX_, textY + 1);
    updateElidedText();
    raise();
}

void EditorPlaceholder::updateVisibility() {
    if (!editor_) {
        return;
    }
    setVisible(editor_->text().isEmpty() && !editor_->hasFocus());
}

bool EditorPlaceholder::eventFilter(QObject *watched, QEvent *event) {
    if (watched == editor_) {
        switch (event->type()) {
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            updateVisibility();
            break;
        case QEvent::Resize:
            updatePosition();
            break;
        case QEvent::FontChange:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
            updateElidedText();
            break;
        case QEvent::Wheel:
            // Zoom may have changed, check after event is processed
            QTimer::singleShot(0, this, &EditorPlaceholder::updateZoom);
            break;
        default:
            break;
        }
    }
    return QLabel::eventFilter(watched, event);
}

void EditorPlaceholder::updateZoom() {
    if (!editor_) {
        return;
    }
    
    const int currentZoom = static_cast<int>(editor_->SendScintilla(QsciScintilla::SCI_GETZOOM));
    if (currentZoom != lastZoom_) {
        lastZoom_ = currentZoom;
        applyZoomedFont();
        updatePosition();
    }
}

void EditorPlaceholder::applyZoomedFont() {
    if (baseFont_.pointSize() <= 0) {
        return;
    }
    
    QFont zoomedFont = baseFont_;
    const int newSize = std::max(1, baseFont_.pointSize() + lastZoom_);
    zoomedFont.setPointSize(newSize);
    setFont(zoomedFont);
    updateElidedText();
}

void EditorPlaceholder::updateElidedText() {
    if (!editor_) {
        return;
    }

    const int available = std::max(0, editor_->viewport()->width() - textX_ - 4);
    const QFontMetrics metrics(font());
    const QString elided = elideWithDots(fullText_, available);
    QLabel::setText(elided);
    adjustSize();
}

QString EditorPlaceholder::elideWithDots(const QString &text, int maxWidth) const {
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
