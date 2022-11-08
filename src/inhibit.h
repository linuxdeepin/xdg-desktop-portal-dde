#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class InhibitPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Inhibit")

public:
    explicit InhibitPortal(QObject *parent);
    ~InhibitPortal() = default;

public slots:
    void
    Inhibit(const QDBusObjectPath &handle, const QString &app_id, const QString &window, uint flags, const QVariantMap &options);
};
