// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "foreigntoplevellist.h"

ForeignToplevelList::ForeignToplevelList(QObject *parent)
    : QWaylandClientExtensionTemplate<ForeignToplevelList>(1)
    , QtWayland::ext_foreign_toplevel_list_v1()
{
}

uint32_t ForeignToplevelList::version()
{
    return QtWayland::ext_foreign_toplevel_list_v1::version();
}

void ForeignToplevelList::ext_foreign_toplevel_list_v1_toplevel(struct ::ext_foreign_toplevel_handle_v1 *toplevel)
{
    Q_EMIT toplevelAdded(new ForeignToplevelHandle(toplevel));
}

void ForeignToplevelList::ext_foreign_toplevel_list_v1_finished()
{
    Q_EMIT finished();
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
