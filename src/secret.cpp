#include "secret.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDESecret, "xdg-dde-secret")

SecretPortal::SecretPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qCDebug(XdgDesktopDDESecret) << "secret init";
}

uint SecretPortal::RetrieveSecret(const QDBusObjectPath &handle,
                                  const QString &app_id,
                                  const QDBusUnixFileDescriptor &fd,
                                  const QVariantMap &options,
                                  QVariantMap &result)
{
    qCDebug(XdgDesktopDDESecret) << "RetrieveSecret";
    return 1;
}
