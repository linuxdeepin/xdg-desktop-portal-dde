// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "foreignparent.h"

#include "qwayland-xdg-foreign-unstable-v2.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#include <private/qwaylandclientextension_p.h>

#include <utility>

Q_LOGGING_CATEGORY(waylandForeignParentLog, "dde.portal.wayland.foreignparent")

namespace
{

class ForeignParentManager final
    : public QWaylandClientExtensionTemplate<ForeignParentManager>
    , public QtWayland::zxdg_importer_v2
{
public:
    ForeignParentManager()
        : QWaylandClientExtensionTemplate<ForeignParentManager>(1)
    {
    }

    ~ForeignParentManager() override
    {
        if (isInitialized()) {
            destroy();
        }
    }
};

class ImportedParent final : public QObject, public QtWayland::zxdg_imported_v2
{
public:
    ImportedParent(struct ::zxdg_imported_v2 *object, QWindow *window, struct ::wl_surface *surface)
        : QObject(window)
        , QtWayland::zxdg_imported_v2(object)
    {
        set_parent_of(surface);
    }

    ~ImportedParent() override
    {
        if (isInitialized()) {
            destroy();
        }
    }

protected:
    void zxdg_imported_v2_destroyed() override
    {
        deleteLater();
    }
};

QPointer<ForeignParentManager> foreignParentManager()
{
    static QPointer<ForeignParentManager> manager;
    if (!manager) {
        manager = new ForeignParentManager;
        manager->setParent(qApp);
    }
    return manager;
}

class ForeignParentBinding final : public QObject
{
public:
    ForeignParentBinding(QWindow *window, QString handle)
        : QObject(window)
        , m_window(window)
        , m_manager(foreignParentManager())
        , m_handle(std::move(handle))
    {
        if (!m_manager) {
            deleteLater();
            return;
        }

        connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
            if (visible) {
                scheduleAttempt();
            }
        });
        connect(m_manager, &ForeignParentManager::activeChanged, this, [this] {
            scheduleAttempt();
        });
        scheduleAttempt();
    }

private:
    void scheduleAttempt()
    {
        if (m_attemptScheduled) {
            return;
        }
        m_attemptScheduled = true;

        // QWindow::visibleChanged(true) is emitted before the platform
        // window's setVisible() call. Queue the attempt so Qt Wayland can
        // create the xdg_surface and xdg_toplevel first.
        QTimer::singleShot(0, this, [this] {
            m_attemptScheduled = false;
            attemptBinding();
        });
    }

    void attemptBinding()
    {
        if (!m_window || !m_window->isVisible() || !m_manager || !m_manager->isActive()) {
            return;
        }

        auto nativeInterface = qGuiApp->platformNativeInterface();
        if (!nativeInterface) {
            qCWarning(waylandForeignParentLog)
                    << "No native interface is available for Wayland parent binding";
            deleteLater();
            return;
        }

        // A wl_surface can exist before Qt assigns it an xdg_toplevel role.
        // Passing such a surface to zxdg_imported_v2.set_parent_of is a fatal
        // protocol error, so verify the exact role instead of inferring it
        // from window visibility or the existence of a wl_surface.
        if (!nativeInterface->nativeResourceForWindow(
                    QByteArrayLiteral("xdg_toplevel"), m_window)) {
            qCWarning(waylandForeignParentLog)
                    << "Portal window has no xdg_toplevel role; skipping parent binding";
            deleteLater();
            return;
        }

        auto surface = static_cast<struct ::wl_surface *>(
                nativeInterface->nativeResourceForWindow(
                        QByteArrayLiteral("surface"), m_window));
        if (!surface) {
            qCWarning(waylandForeignParentLog)
                    << "No Wayland surface is available for portal dialog";
            deleteLater();
            return;
        }

        auto imported = m_manager->import_toplevel(m_handle);
        if (!imported) {
            qCWarning(waylandForeignParentLog)
                    << "Failed to import parent window handle";
            deleteLater();
            return;
        }

        new ImportedParent(imported, m_window, surface);
        deleteLater();
    }

    QPointer<QWindow> m_window;
    QPointer<ForeignParentManager> m_manager;
    QString m_handle;
    bool m_attemptScheduled = false;
};

}

bool WaylandForeignParent::setParent(QWindow *window, const QString &handle)
{
    if (!window || handle.isEmpty()
        || QGuiApplication::platformName() != QLatin1String("wayland")) {
        return false;
    }

    new ForeignParentBinding(window, handle);
    return true;
}
