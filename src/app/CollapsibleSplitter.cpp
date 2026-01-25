#include "app/CollapsibleSplitter.h"

#include <QList>
#include <QResizeEvent>
#include <algorithm>

CollapsibleSplitter::CollapsibleSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent) {
    setChildrenCollapsible(true);
    setHandleWidth(1);
    connect(this, &QSplitter::splitterMoved, this, &CollapsibleSplitter::onSplitterMoved);
}

void CollapsibleSplitter::setCollapsibleIndex(int index) {
    collapsibleIndex_ = index;
}

void CollapsibleSplitter::setMinimumPanelWidth(int width) {
    minPanelWidth_ = width;
}

void CollapsibleSplitter::setPreferredWidth(int width) {
    preferredWidth_ = width;
    savedWidth_ = std::max(width, minPanelWidth_);
}

bool CollapsibleSplitter::isCollapsed() const {
    return collapsed_;
}

void CollapsibleSplitter::collapse() {
    if (collapsed_) {
        return;
    }

    // Save current width before collapsing
    const QList<int> currentSizes = sizes();
    if (collapsibleIndex_ < currentSizes.size() && 
        currentSizes.at(collapsibleIndex_) >= minPanelWidth_) {
        savedWidth_ = currentSizes.at(collapsibleIndex_);
    }

    collapsed_ = true;

    if (QWidget *w = widget(collapsibleIndex_)) {
        w->setEnabled(false);
        w->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    QList<int> newSizes = currentSizes;
    if (collapsibleIndex_ < newSizes.size()) {
        newSizes[collapsibleIndex_] = 0;
    }
    
    ignoreMove_ = true;
    setSizes(newSizes);
    ignoreMove_ = false;

    emit collapsedChanged(true);
}

void CollapsibleSplitter::expand() {
    if (!collapsed_) {
        return;
    }

    collapsed_ = false;

    if (QWidget *w = widget(collapsibleIndex_)) {
        w->setEnabled(true);
        w->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }

    QList<int> newSizes = sizes();
    if (collapsibleIndex_ < newSizes.size()) {
        newSizes[collapsibleIndex_] = std::max(savedWidth_, minPanelWidth_);
    }

    ignoreMove_ = true;
    setSizes(newSizes);
    ignoreMove_ = false;

    emit collapsedChanged(false);
}

void CollapsibleSplitter::toggleCollapse() {
    if (collapsed_) {
        expand();
    } else {
        collapse();
    }
}

void CollapsibleSplitter::resizeEvent(QResizeEvent *event) {
    QSplitter::resizeEvent(event);
}

void CollapsibleSplitter::onSplitterMoved(int /*pos*/, int /*index*/) {
    if (ignoreMove_) {
        return;
    }
    updateCollapseState();
}

void CollapsibleSplitter::updateCollapseState() {
    const QList<int> currentSizes = sizes();
    if (collapsibleIndex_ >= currentSizes.size()) {
        return;
    }

    const int width = currentSizes.at(collapsibleIndex_);

    if (collapsed_) {
        // Expand when dragged past minimum
        if (width >= minPanelWidth_) {
            expand();
        }
    } else {
        // Collapse when shrunk below minimum
        if (width < minPanelWidth_) {
            collapse();
        } else {
            savedWidth_ = width;
        }
    }
}
