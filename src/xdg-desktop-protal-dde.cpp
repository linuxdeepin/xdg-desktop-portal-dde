#include "ddesktopprotal.h"

#include <QApplication>
#include <QDBusConnection>
#include <cstdio>
#include <QDebug>
#include <qstringliteral.h>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDE, "xdg-dde")

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_DisableSessionManager);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.dde"))) {
        DDestkopPortal *ddesktopprotal = new DDestkopPortal(&a);
        if (sessionBus.registerObject(
                QStringLiteral("/org/freedesktop/portal/desktop"), ddesktopprotal, QDBusConnection::ExportAdaptors)) {
            qCDebug(XdgDesktopDDE) << "portal started";
        }
    } else {
        qCDebug(XdgDesktopDDE) << "Another portal is starting";
        return 1;
    }
    return a.exec();
}
