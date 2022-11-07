#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <qdbusextratypes.h>
#include <qobjectdefs.h>

class SettingsPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")
    Q_PROPERTY(uint version READ version CONSTANT)
    inline uint version() const { return 1; }

public:
    explicit SettingsPortal(QObject *parent);
    ~SettingsPortal() = default;

public slots:
    void ReadAll(const QStringList &groups);
    void Read(const QString &group, const QString &key);

signals:
    void SettingChanged(const QString &group, const QString &key, const QDBusVariant &value);
};
