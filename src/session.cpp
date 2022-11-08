#include "session.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgDesktopDDESession, "xdg-dde-session")

SessionPortal::SessionPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void SessionPortal::Close()
{
    qCDebug(XdgDesktopDDESession) << "Closed";
}
