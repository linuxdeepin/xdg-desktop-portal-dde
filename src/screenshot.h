#pragma once
#include <QDBusObjectPath>
#include <QDBusAbstractAdaptor>
#include <qobjectdefs.h>
#include <QDBusInterface>

class ScreenshotPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Screenshot")
public:
    struct ColorRGB
    {
        double red;
        double green;
        double blue;
    };
    explicit ScreenshotPortal(QObject *parent);
    ~ScreenshotPortal() = default;
public slots:
    uint PickColor(const QDBusObjectPath &handle,
                   const QString &app_id,
                   const QString &parent_window,
                   const QVariantMap &options,
                   QVariantMap &result);
    uint Screenshot(const QDBusObjectPath &handle,
                    const QString &app_id,
                    const QString &parent_window,
                    const QString &options,
                    QVariantMap &results);
};
