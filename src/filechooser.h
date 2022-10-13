#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <qobjectdefs.h>

class FileChooserProtal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
public:
    explicit FileChooserProtal(QObject *parent);
    ~FileChooserProtal() = default;
public slots:
    uint OpenFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);
    uint SaveFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);
};
