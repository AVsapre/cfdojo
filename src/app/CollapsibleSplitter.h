#pragma once

#include <QSplitter>

class CollapsibleSplitter : public QSplitter {
    Q_OBJECT

public:
    explicit CollapsibleSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setCollapsibleIndex(int index);
    void setMinimumPanelWidth(int width);  // Fixed minimum; collapses at half this
    void setPreferredWidth(int width);     // Initial/restore width
    
    bool isCollapsed() const;

public slots:
    void collapse();
    void expand();
    void toggleCollapse();

signals:
    void collapsedChanged(bool collapsed);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSplitterMoved(int pos, int index);

private:
    void updateCollapseState();

    int collapsibleIndex_ = 1;
    int minPanelWidth_ = 170;     // Collapse threshold
    int preferredWidth_ = 240;
    int savedWidth_ = 240;
    bool collapsed_ = false;
    bool ignoreMove_ = false;
};
