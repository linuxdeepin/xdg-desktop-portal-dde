#pragma once
#include "screenshot.h"
#include "background.h"
#include "filechooser.h"
#include "wallpaper.h"
#include "notification.h"
#include <QDBusContext>
#include <QObject>

class DDestkopPortal : public QObject, public QDBusContext
{
    Q_OBJECT
public:
    explicit DDestkopPortal(QObject *parent = nullptr);
    ~DDestkopPortal() = default;

private:
    ScreenshotPortal *m_screenshot = nullptr;
    BackgroundPortal *m_background = nullptr;
    WallPaperPortal *const m_wallpaper;
    NotificationProtal *const m_notification;
    FileChooserProtal *const m_filechooser;
};
