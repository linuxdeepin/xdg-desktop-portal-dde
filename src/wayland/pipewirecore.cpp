// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "pipewirecore.h"
#include "loggings.h"

#include <QLoggingCategory>
#include <QSocketNotifier>

PipeWireCore::PipeWireCore(QObject *parent)
    : QObject(parent)
{
    pw_init(nullptr, nullptr);
    m_pwCoreEvents.version = PW_VERSION_CORE_EVENTS;
    m_pwCoreEvents.error = &PipeWireCore::onCoreError;
    init();
}

PipeWireCore::~PipeWireCore()
{
    if (m_pwMainLoop) {
        pw_loop_leave(m_pwMainLoop);
    }

    if (m_pwCore) {
        pw_core_disconnect(m_pwCore);
    }

    if (m_pwContext) {
        pw_context_destroy(m_pwContext);
    }

    if (m_pwMainLoop) {
        pw_loop_destroy(m_pwMainLoop);
    }

    pw_deinit();
}

bool PipeWireCore::init()
{
    m_pwMainLoop = pw_loop_new(nullptr);
    if (!m_pwMainLoop) {
        qCCritical(PIPEWIRE, "Failed to create PipeWire loop: %s", strerror(errno));
        m_error = QString("Failed to start main PipeWire loop");
        return false;
    }
    pw_loop_enter(m_pwMainLoop);

    m_notifier = new QSocketNotifier(pw_loop_get_fd(m_pwMainLoop), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        int result = pw_loop_iterate(m_pwMainLoop, 0);
        if (result < 0) {
            qCCritical(PIPEWIRE) << "pipewire_loop_iterate failed: " << result;
        }
    });

    m_pwContext = pw_context_new(m_pwMainLoop, nullptr, 0);
    if (!m_pwContext) {
        qCCritical(PIPEWIRE) << "Failed to create PipeWire context";
        m_error = QString("Failed to create PipeWire context");
        return false;
    }

    m_pwCore = pw_context_connect(m_pwContext, nullptr, 0);
    if (!m_pwCore) {
        qCCritical(PIPEWIRE) << "Failed to connect PipeWire context";
        m_error = QString("Failed to connect PipeWire context");
        return false;
    }

    if (pw_loop_iterate(m_pwMainLoop, 0) < 0) {
        qCCritical(PIPEWIRE) << "Failed to start main PipeWire loop";
        m_error = QString("Failed to start main PipeWire loop");
        return false;
    }

    pw_core_add_listener(m_pwCore, &m_coreListener, &m_pwCoreEvents, this);
    m_valid = true;
    return true;
}

void PipeWireCore::onCoreError(void *data,
                               uint32_t id,
                               int seq,
                               int res,
                               const char *message)
{
    qCCritical(PIPEWIRE) << "PipeWire remote error: " << message;
    if (id == PW_ID_CORE && res == -EPIPE) {
        PipeWireCore *pw = static_cast<PipeWireCore *>(data);
        pw->m_valid = false;
        Q_EMIT pw->pipewireFailed(QString::fromUtf8(message));
    }
}

bool PipeWireCore::isValid() const
{
    return m_valid;
}
