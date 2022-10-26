#include "trash.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDETrash, "xdg-dde-trash")

TrashPortal::TrashPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

uint TrashPortal::TrashFile(const QDBusObjectPath &handle, const QString &app_id, const QDBusUnixFileDescriptor &file)
{
    qCDebug(XdgDesktopDDETrash) << "remove file to trash";
    return 1;
}
