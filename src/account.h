#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class AccountPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Account")

public:
    explicit AccountPortal(QObject *parent);
    ~AccountPortal() = default;

public slots:
    uint GetUserInformation(const QDBusObjectPath &handle,
                            const QString &app_id,
                            const QString &window,
                            const QVariantMap &options,
                            QVariantMap &results);
};
