// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "access.h"

#include "accessdialog.h"
#include "request.h"
#include "utils.h"

#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDialog>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTimer>

#if QT_CONFIG(wayland)
#  include <QtGui/qguiapplication_platform.h>
#  include <wayland-client.h>
#endif

#include <cerrno>
#include <functional>
#include <utility>

Q_LOGGING_CATEGORY(XdgDestkopDDEAccess, "xdg-dde-access")

namespace
{

// Treeland keeps an unmapped toplevel's snapshot visible for its default
// 400 ms close animation. Leave scheduling margin before allowing screencopy
// to request its next output commit.
constexpr int WindowWithdrawalGracePeriodMs = 500;

#if QT_CONFIG(wayland)

class WaylandSyncBarrier final : public QObject
{
public:
    using Completion = std::function<void()>;

    static void wait(wl_display *display,
                     QObject *context,
                     Completion completion,
                     Completion failure)
    {
        auto barrier = new WaylandSyncBarrier(context, std::move(completion));
        if (barrier->start(display)) {
            return;
        }

        delete barrier;
        failure();
    }

    ~WaylandSyncBarrier() override
    {
        if (m_callback) {
            wl_callback_destroy(m_callback);
        }
    }

private:
    explicit WaylandSyncBarrier(QObject *parent, Completion completion)
        : QObject(parent)
        , m_completion(std::move(completion))
    {
    }

    bool start(wl_display *display)
    {
        if (!display) {
            return false;
        }

        m_callback = wl_display_sync(display);
        if (!m_callback || wl_callback_add_listener(m_callback, &s_listener, this) < 0) {
            return false;
        }

        // EAGAIN only means that Qt's Wayland event loop must flush again once
        // the socket becomes writable. Other errors cannot produce a useful
        // synchronization point, so fail closed instead of taking a screenshot
        // while the permission dialog may still be mapped.
        if (wl_display_flush(display) < 0 && errno != EAGAIN) {
            qCWarning(XdgDestkopDDEAccess)
                    << "Failed to flush the Wayland window-withdrawal barrier";
            return false;
        }
        return true;
    }

    static void handleDone(void *data, wl_callback *callback, uint32_t serial)
    {
        Q_UNUSED(serial)

        auto barrier = static_cast<WaylandSyncBarrier *>(data);
        if (callback != barrier->m_callback) {
            return;
        }

        wl_callback_destroy(barrier->m_callback);
        barrier->m_callback = nullptr;

        auto completion = std::move(barrier->m_completion);
        barrier->deleteLater();
        completion();
    }

    static const wl_callback_listener s_listener;

    wl_callback *m_callback = nullptr;
    Completion m_completion;
};

const wl_callback_listener WaylandSyncBarrier::s_listener = {
    WaylandSyncBarrier::handleDone,
};

#endif

void sendAccessReply(const QDBusMessage &message,
                     uint response,
                     const AccessDialogTypes::Choices &choices = {})
{
    QVariantMap results;
    results.insert(QStringLiteral("choices"), QVariant::fromValue(choices));
    const auto reply = message.createReply(
            QVariantList{QVariant::fromValue(response), results});
    if (!QDBusConnection::sessionBus().send(reply)) {
        qCWarning(XdgDestkopDDEAccess) << "Failed to send access dialog response";
    }
}

void sendAccessReplyAfterWindowWithdrawal(
        QObject *context,
        const QDBusMessage &message,
        uint response,
        const AccessDialogTypes::Choices &choices)
{
    // Run after the dialog's deferred deletion has completed. This makes the
    // Wayland sync request follow the surface unmap/destruction requests on the
    // same connection rather than merely following QDialog::finished().
    QTimer::singleShot(0, context, [context, message, response, choices] {
        const auto sendReply = [message, response, choices] {
            sendAccessReply(message, response, choices);
        };

        if (QGuiApplication::platformName() != QLatin1String("wayland")) {
            QGuiApplication::sync();
            sendReply();
            return;
        }

#if QT_CONFIG(wayland)
        auto waylandApplication = qGuiApp->nativeInterface<
                QNativeInterface::QWaylandApplication>();
        if (!waylandApplication) {
            qCWarning(XdgDestkopDDEAccess)
                    << "Cannot synchronize access dialog withdrawal with Wayland";
            sendAccessReply(message, 2);
            return;
        }

        WaylandSyncBarrier::wait(
                waylandApplication->display(),
                context,
                [context, response, sendReply] {
                    if (response == 0) {
                        QTimer::singleShot(
                                WindowWithdrawalGracePeriodMs, context, sendReply);
                    } else {
                        sendReply();
                    }
                },
                [message] {
                    qCWarning(XdgDestkopDDEAccess)
                            << "Cannot establish the Wayland window-withdrawal barrier";
                    sendAccessReply(message, 2);
                });
#else
        qCWarning(XdgDestkopDDEAccess)
                << "Wayland platform detected without Wayland client support";
        sendAccessReply(message, 2);
#endif
    });
}

}

AccessPortal::AccessPortal(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qDBusRegisterMetaType<AccessDialogTypes::Choice>();
    qDBusRegisterMetaType<AccessDialogTypes::Choices>();
    qDBusRegisterMetaType<AccessDialogTypes::Option>();
    qDBusRegisterMetaType<AccessDialogTypes::OptionList>();
    qCDebug(XdgDestkopDDEAccess) << "Access portal initialized";
}

void AccessPortal::AccessDialog(const QDBusObjectPath &handle,
                                const QString &app_id,
                                const QString &parent_window,
                                const QString &title,
                                const QString &subtitle,
                                const QString &body,
                                const QVariantMap &options,
                                const QDBusMessage &message,
                                uint &replyResponse,
                                QVariantMap &replyResults)
{
    Q_UNUSED(replyResponse)
    Q_UNUSED(replyResults)

    qCDebug(XdgDestkopDDEAccess) << "Access dialog requested by" << app_id
                                 << "with handle" << handle.path();
    message.setDelayedReply(true);

    AccessDialogTypes::OptionList choices;
    if (options.contains(QStringLiteral("choices"))) {
        choices = qdbus_cast<AccessDialogTypes::OptionList>(
                options.value(QStringLiteral("choices")));
    }

    auto dialog = new ::AccessDialog(title, subtitle, body, options, choices);
    Utils::setParentWindow(dialog, parent_window);

    auto request = new Request(handle, QVariant(), dialog);
    if (!request->isRegistered()) {
        sendAccessReply(message, 2);
        dialog->deleteLater();
        return;
    }
    connect(request, &Request::closeRequested, dialog, &QDialog::reject);
    connect(dialog,
            &QDialog::finished,
            this,
            [dialog, request, message, this](int result) {
                request->unregister();
                const uint response = result == QDialog::Accepted ? 0 : 1;
                const auto selectedChoices = dialog->selectedChoices();

                // QDialog::finished() is emitted after the widget is hidden, but on
                // Wayland that does not mean the compositor has processed the unmap.
                // Destroy the native window first and reply only after the display
                // synchronization point acknowledges all preceding requests.
                connect(dialog,
                        &QObject::destroyed,
                        this,
                        [this, message, response, selectedChoices] {
                            sendAccessReplyAfterWindowWithdrawal(
                                    this, message, response, selectedChoices);
                        });
                dialog->hide();
                dialog->deleteLater();
            },
            Qt::SingleShotConnection);
    dialog->open();
}
