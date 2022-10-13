#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <qobjectdefs.h>

class WallPaperPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.WallPaper")
public:
    explicit WallPaperPortal(QObject *parent);
    ~WallPaperPortal() = default;
public slots:
    uint SetWallpaperUrI(const QDBusObjectPath &handle,
                         const QString &app_id,
                         const QString &parent_window,
                         const QString &url,
                         const QVariantMap &options);
};
