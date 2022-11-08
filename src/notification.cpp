#include "notification.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDENotification, "xdg-dde-notification")

// TODO: if needed
NotificationProtal::NotificationProtal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDENotification) << "init NotificationProtal";
}

void NotificationProtal::AddNotification(const QString &app_id, const QString &id, const QVariantMap &notification)
{
    qCDebug(XdgDesktopDDENotification) << "notification add start";
}

void NotificationProtal::RemoveNotification(const QString &app_id, const QString &id)
{
    qCDebug(XdgDesktopDDENotification) << "notification remove start";
}
