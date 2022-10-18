#include "notification.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDENotification, "xdg-dde-notification")

NotificationProtal::NotificationProtal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void NotificationProtal::AddNotification(const QString &app_id, const QString &id, const QVariantMap &notification)
{
    qCDebug(XdgDesktopDDENotification) << "notification add start";
}

void NotificationProtal::RemoveNotification(const QString &app_id, const QString &id)
{
    qCDebug(XdgDesktopDDENotification) << "notification remove start";
}
