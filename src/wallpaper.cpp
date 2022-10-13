#include "wallpaper.h"

WallPaperPortal::WallPaperPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}
uint WallPaperPortal::SetWallpaperUrI(const QDBusObjectPath &handle,
                                const QString &app_id,
                                const QString &parent_window,
                                const QString &url,
                                const QVariantMap &options)
{
    return 1;
}
