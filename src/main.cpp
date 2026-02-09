#include <QApplication>
#include <QFontMetricsF>
#include <QPlainTextEdit>
#include "app/MainWindow.h"
#include "Version.h"

#include <csignal>

namespace {
/// Event filter that sets tabStopDistance to 4 spaces on every
/// QPlainTextEdit whenever it is shown or its font changes.
class TabStopFilter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::Show ||
            event->type() == QEvent::FontChange) {
            if (auto *edit = qobject_cast<QPlainTextEdit *>(obj)) {
                const QFontMetricsF fm(edit->font());
                edit->setTabStopDistance(fm.horizontalAdvance(' ') * 4);
            }
        }
        return false;
    }
};
} // namespace

int main(int argc, char *argv[]) {
    // Ignore SIGPIPE so writing to a closed socket/pipe (e.g. Competitive
    // Companion disconnect, QProcess exit) doesn't crash the app.
    std::signal(SIGPIPE, SIG_IGN);

    QApplication app(argc, argv);
    app.setOrganizationName("CF Dojo");
    app.setApplicationName("CF Dojo");
    app.setApplicationVersion(CFDojo::kVersion);

    auto *tabFilter = new TabStopFilter(&app);
    app.installEventFilter(tabFilter);

    MainWindow window;
    window.show();

    return app.exec();
}
