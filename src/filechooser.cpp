#include "filechooser.h"

FileChooserProtal::FileChooserProtal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

uint FileChooserProtal::OpenFile(const QDBusObjectPath &handle,
                                 const QString &app_id,
                                 const QString &parent_window,
                                 const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results)
{
    return 1;
}
uint FileChooserProtal::SaveFile(const QDBusObjectPath &handle,
                                 const QString &app_id,
                                 const QString &parent_window,
                                 const QString &title,
                                 const QVariantMap &options,
                                 QVariantMap &results)
{
    return 1;
}
