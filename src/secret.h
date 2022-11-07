#pragma once

#include <QDBusObjectPath>
#include <QDBusAbstractAdaptor>
#include <qdbusargument.h>

class SecretPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Secret")

    Q_PROPERTY(uint version READ version CONSTANT)
    inline uint version() const { return 1; }

public:
    SecretPortal(QObject *parent);
    ~SecretPortal() = default;

public slots:
    uint RetrieveSecret(const QDBusObjectPath &handle,
                        const QString &app_id,
                        const QDBusUnixFileDescriptor &fd,
                        const QVariantMap &options,
                        QVariantMap &result);
};
