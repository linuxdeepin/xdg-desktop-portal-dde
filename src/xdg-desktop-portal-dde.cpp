// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ddesktopportal.h"

#include <QApplication>
#include <QDBusConnection>
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
        DDesktopPortal *desktopPortal = new DDesktopPortal(&a);
        if (sessionBus.registerObject(
                QStringLiteral("/org/freedesktop/portal/desktop"), desktopPortal, QDBusConnection::ExportAdaptors)) {
            qCDebug(XdgDesktopDDE) << "portal started";
        }
    } else {
        qCDebug(XdgDesktopDDE) << "Another portal is starting";
        return 1;
    }

    return a.exec();
}
