#pragma once

#include <QString>

class QFileSystemModel;
class QTreeView;
class QWidget;

class FileExplorerBuilder {
public:
    struct Widgets {
        QWidget *panel = nullptr;
        QTreeView *tree = nullptr;
        QFileSystemModel *model = nullptr;
    };

    Widgets build(QWidget *parent, const QString &rootPath) const;
};
