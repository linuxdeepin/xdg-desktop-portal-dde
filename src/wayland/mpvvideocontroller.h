// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <mpv/client.h>

#include <QObject>
#include <QVariant>

class MpvVideoController : public QObject
{
    Q_OBJECT
public:
    explicit MpvVideoController(QObject *parent = nullptr);
    QString getError(int error);
    static void mpvEvents(void *ctx);
    void eventHandler();
    mpv_handle *mpv() const;

public Q_SLOTS:
    void init();
    void observeProperty(const QString &property, mpv_format format, uint64_t id = 0);
    int unobserveProperty(uint64_t id);
    int setProperty(const QString &property, const QVariant &value);
    int setPropertyAsync(const QString &property, const QVariant &value, int id = 0);
    QVariant getProperty(const QString &property);
    int getPropertyAsync(const QString &property, int id = 0);
    QVariant command(const QVariant &params);
    int commandAsync(const QVariant &params, int id = 0);

Q_SIGNALS:
    void propertyChanged(const QString &property, const QVariant &value);
    void asyncReply(const QVariant &data, mpv_event event);
    void fileStarted();
    void fileLoaded();
    void endFile(QString reason);
    void videoReconfig();

private:
    mpv_node_list *createList(mpv_node *dst, bool is_map, int num);
    void setNode(mpv_node *dst, const QVariant &src);
    void freeNode(mpv_node *dst);
    QVariant nodeToVariant(const mpv_node *node);

private:
    mpv_handle *m_mpv = nullptr;
};
