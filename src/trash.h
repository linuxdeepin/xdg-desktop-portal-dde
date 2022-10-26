#pragma once

#include <QDBusUnixFileDescriptor>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class TrashPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Trash")
public:
    explicit TrashPortal(QObject *parent);
    ~TrashPortal() = default;
public slots:
    uint TrashFile(const QDBusObjectPath &handle, const QString &app_id, const QDBusUnixFileDescriptor &file);
};
