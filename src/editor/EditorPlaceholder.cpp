#include "editor/EditorPlaceholder.h"

#include <QApplication>
#include <QEvent>
#include <Qsci/qsciscintilla.h>

EditorPlaceholder::EditorPlaceholder(QsciScintilla *editor, const QString &text)
    : QLabel(text, editor), editor_(editor) {
    setObjectName("EditorPlaceholder");
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setContentsMargins(0, 0, 0, 0);
    adjustSize();
    
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
    
    updatePosition();
    updateVisibility();
    raise();
}

void EditorPlaceholder::updatePosition() {
    if (!editor_) {
        return;
    }

    const long pos = static_cast<long>(
        editor_->SendScintilla(QsciScintilla::SCI_GETCURRENTPOS));
    
    const int textX = static_cast<int>(editor_->SendScintilla(
        QsciScintilla::SCI_POINTXFROMPOSITION,
        static_cast<unsigned long>(0),
        static_cast<long>(pos)));
    
    const int textY = static_cast<int>(editor_->SendScintilla(
        QsciScintilla::SCI_POINTYFROMPOSITION,
        static_cast<unsigned long>(0),
        static_cast<long>(pos)));

    const int caretWidth = static_cast<int>(
        editor_->SendScintilla(QsciScintilla::SCI_GETCARETWIDTH));
    
    move(textX + caretWidth, textY);
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
        default:
            break;
        }
    }
    return QLabel::eventFilter(watched, event);
}
