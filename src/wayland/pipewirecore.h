// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QObject>

#include <pipewire/pipewire.h>
#include <spa/utils/hook.h>

class QSocketNotifier;
class ScreenCastContext;
class PipeWireStream;

class PipeWireCore : public QObject
{
    Q_OBJECT
public:
    PipeWireCore(QObject *parent = nullptr);
    ~PipeWireCore() override;

    bool init();
    bool isValid() const;

    static void onCoreError(void *data,
                            uint32_t id,
                            int seq,
                            int res,
                            const char *message);

Q_SIGNALS:
    void pipewireFailed(const QString &message);

private:
    friend class ScreenCastContext;
    friend class PipeWireStream;

    struct pw_core *m_pwCore = nullptr;
    struct pw_context *m_pwContext = nullptr;
    struct pw_loop *m_pwMainLoop = nullptr;
    spa_hook m_coreListener;
    QString m_error;
    QSocketNotifier *m_notifier = nullptr;

    pw_core_events m_pwCoreEvents = {};
    bool m_valid = false;
};
