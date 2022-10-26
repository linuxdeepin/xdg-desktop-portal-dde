#include "ddesktopprotal.h"

DDestkopPortal::DDestkopPortal(QObject *parent)
    : QObject(parent)
    , m_filechooser(new FileChooserProtal(this))
    , m_wallpaper(new WallPaperPortal(this))
    , m_notification(new NotificationProtal(this))
    , m_trash(new TrashPortal(this))
{
    const QByteArray xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop.compare("Deepin", Qt::CaseInsensitive) == 0) {
        m_screenshot = new ScreenshotPortal(this);
        m_screencast = new ScreenCastPortal(this);
        m_background = new BackgroundPortal(this);
    }
}
