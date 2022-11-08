#pragma once

#include <QDBusObjectPath>
#include <QDBusAbstractAdaptor>

class SessionPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Session")
    Q_PROPERTY(uint version READ version CONSTANT)
    inline uint version() const { return 1; }

public:
    explicit SessionPortal(QObject *parent);
    ~SessionPortal() = default;

public slots:
    void Close();

signals:
    void Closed();
};
