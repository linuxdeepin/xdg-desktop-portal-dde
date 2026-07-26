// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "foreigntoplevellist.h"

#include <QCoreApplication>
#include <QMetaMethod>

ForeignToplevelList::ForeignToplevelList(QObject *parent)
    : QWaylandClientExtensionTemplate<ForeignToplevelList>(1)
    , QtWayland::ext_foreign_toplevel_list_v1()
{
    setParent(parent);
    connect(this, &ForeignToplevelList::activeChanged, this, [this] {
        if (!isActive() && m_deleteWhenFinished && !m_finished) {
            m_finished = true;
            deleteLater();
        }
    });
}

ForeignToplevelList::~ForeignToplevelList()
{
    if (isInitialized()) {
        destroy();
    }
}

ForeignToplevelListPtr createForeignToplevelList()
{
    return ForeignToplevelListPtr(new ForeignToplevelList,
                                  [](ForeignToplevelList *list) {
        list->releaseAfterFinished();
    });
}

uint32_t ForeignToplevelList::version()
{
    return QtWayland::ext_foreign_toplevel_list_v1::version();
}

void ForeignToplevelList::requestStop()
{
    if (m_stopRequested || m_finished) {
        return;
    }

    m_stopRequested = true;
    if (isInitialized() && isActive()) {
        stop();
    } else {
        m_finished = true;
    }
}

void ForeignToplevelList::releaseAfterFinished()
{
    if (!QCoreApplication::instance() || QCoreApplication::closingDown()) {
        delete this;
        return;
    }

    m_deleteWhenFinished = true;
    requestStop();
    if (m_finished) {
        deleteLater();
    }
}

void ForeignToplevelList::ext_foreign_toplevel_list_v1_toplevel(struct ::ext_foreign_toplevel_handle_v1 *toplevel)
{
    auto *handle = new ForeignToplevelHandle(toplevel);
    if (!isSignalConnected(QMetaMethod::fromSignal(&ForeignToplevelList::toplevelAdded))) {
        handle->destroy();
        handle->deleteLater();
        return;
    }

    Q_EMIT toplevelAdded(handle);
}

void ForeignToplevelList::ext_foreign_toplevel_list_v1_finished()
{
    m_finished = true;
    Q_EMIT finished();
    if (m_deleteWhenFinished) {
        deleteLater();
    }
}

ForeignToplevelHandle::ForeignToplevelHandle(struct ::ext_foreign_toplevel_handle_v1 *object, QObject *parent)
    : QObject(parent)
    , QtWayland::ext_foreign_toplevel_handle_v1(object)
{
}

void ForeignToplevelHandle::ext_foreign_toplevel_handle_v1_closed()
{
    Q_EMIT closed();
}

void ForeignToplevelHandle::ext_foreign_toplevel_handle_v1_done()
{
    Q_EMIT done();
}

void ForeignToplevelHandle::ext_foreign_toplevel_handle_v1_title(const QString &title)
{
    Q_EMIT titleChanged(title);
}

void ForeignToplevelHandle::ext_foreign_toplevel_handle_v1_app_id(const QString &app_id)
{
    Q_EMIT appIdChanged(app_id);
}

void ForeignToplevelHandle::ext_foreign_toplevel_handle_v1_identifier(const QString &identifier)
{
    Q_EMIT identifierChanged(identifier);
}
