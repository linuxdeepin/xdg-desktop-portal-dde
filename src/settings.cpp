#include "settings.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDESetting, "xdg-dde-settings")

SettingsPortal::SettingsPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void SettingsPortal::Read(const QString &group, const QString &key)
{
    qCDebug(XdgDesktopDDESetting) << "Read " << key;
}

void SettingsPortal::ReadAll(const QStringList &groups)
{
    qCDebug(XdgDesktopDDESetting) << "ReadAll";
}
