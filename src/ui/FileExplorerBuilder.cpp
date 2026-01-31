#include "ui/FileExplorerBuilder.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFileSystemModel>
#include <QLabel>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

FileExplorerBuilder::Widgets FileExplorerBuilder::build(QWidget *parent,
                                                        const QString &rootPath) const {
    Widgets widgets;

    QWidget *panel = new QWidget(parent);
    panel->setObjectName("FileExplorer");

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel("Files", panel);
    titleLabel->setObjectName("FileExplorerTitle");
    layout->addWidget(titleLabel);

    auto *model = new QFileSystemModel(panel);
    model->setRootPath(rootPath);
    model->setFilter(QDir::NoDotAndDotDot | QDir::AllEntries);

    auto *tree = new QTreeView(panel);
    tree->setObjectName("FileExplorerTree");
    tree->setModel(model);
    tree->setRootIndex(model->index(rootPath));
    tree->setHeaderHidden(true);
    tree->setAnimated(true);
    tree->setIndentation(12);
    tree->setMouseTracking(true);
    if (tree->viewport()) {
        tree->viewport()->setMouseTracking(true);
    }
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tree->setColumnHidden(1, true);
    tree->setColumnHidden(2, true);
    tree->setColumnHidden(3, true);

    layout->addWidget(tree);

    widgets.panel = panel;
    widgets.tree = tree;
    widgets.model = model;
    return widgets;
}
