#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class GlobalShotcutProtal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.GlobalShortcuts")
    Q_PROPERTY(uint version READ version CONSTANT)
    inline uint version() const { return 1; }

public:
    explicit GlobalShotcutProtal(QObject *parent);
    ~GlobalShotcutProtal() = default;

public slots:
    uint CreateSession(const QDBusObjectPath &handle,
                       const QDBusObjectPath &session_handle,
                       const QString &app_id,
                       const QVariantMap &options,
                       QVariantMap &results);
    QVariantMap BindShortCuts(const QDBusObjectPath &handle,
                              const QDBusObjectPath &session_handle,
                              const QVariantMap &shortcuts,
                              const QString &parent_window,
                              const QVariantMap &options);
    QVariantMap ListShortCuts(const QDBusObjectPath &handle, const QDBusObjectPath &session_handle);
};
