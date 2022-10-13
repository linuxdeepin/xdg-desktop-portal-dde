#include "notification.h"

NotificationProtal::NotificationProtal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void NotificationProtal::AddNotification(const QString &app_id, const QString &id, const QVariantMap &notification) {}

void NotificationProtal::RemoveNotification(const QString &app_id, const QString &id) {}
