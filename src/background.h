#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>

class BackgroundPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Background")

public:
    explicit BackgroundPortal(QObject *parent);
    ~BackgroundPortal() = default;

public slots:
    bool EnableAutostart(const QString &app_id, const bool &enable, const QStringList commandline, const uint &flags);
    QVariantMap GetAppState();
    uint NotifyBackground(const QDBusObjectPath &handle, const QString &app_id, QString name, QVariantMap &results);

signals:
    void RunnintApplicationsChanged();
};
