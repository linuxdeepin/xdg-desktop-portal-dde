// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef QT_WAYLAND_ZDDE_SCREENCAST_UNSTABLE_V1
#define QT_WAYLAND_ZDDE_SCREENCAST_UNSTABLE_V1

#include "wayland-zkde-screencast-unstable-v1-client-protocol.h"
#include <QByteArray>
#include <QString>

struct wl_registry;

QT_BEGIN_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wmissing-field-initializers")

namespace QtWayland {
    class  zkde_screencast_unstable_v1
    {
    public:
        zkde_screencast_unstable_v1(struct ::wl_registry *registry, int id, int version);
        zkde_screencast_unstable_v1(struct ::zkde_screencast_unstable_v1 *object);
        zkde_screencast_unstable_v1();

        virtual ~zkde_screencast_unstable_v1();

        void init(struct ::wl_registry *registry, int id, int version);
        void init(struct ::zkde_screencast_unstable_v1 *object);

        struct ::zkde_screencast_unstable_v1 *object() { return m_zkde_screencast_unstable_v1; }
        const struct ::zkde_screencast_unstable_v1 *object() const { return m_zkde_screencast_unstable_v1; }
        static zkde_screencast_unstable_v1 *fromObject(struct ::zkde_screencast_unstable_v1 *object);

        bool isInitialized() const;

        static const struct ::wl_interface *interface();

        enum pointer {
            pointer_hidden = 1, // No cursor
            pointer_embedded = 2, // Render the cursor on the stream
            pointer_metadata = 4, // Send metadata about where the cursor is through PipeWire
        };

        struct ::zkde_screencast_stream_unstable_v1 *stream_output(struct ::wl_output *output, uint32_t pointer);
        struct ::zkde_screencast_stream_unstable_v1 *stream_window(const QString &window_uuid, uint32_t pointer);
        void destroy();

    private:
        struct ::zkde_screencast_unstable_v1 *m_zkde_screencast_unstable_v1;
    };

    class  zkde_screencast_stream_unstable_v1
    {
    public:
        zkde_screencast_stream_unstable_v1(struct ::wl_registry *registry, int id, int version);
        zkde_screencast_stream_unstable_v1(struct ::zkde_screencast_stream_unstable_v1 *object);
        zkde_screencast_stream_unstable_v1();

        virtual ~zkde_screencast_stream_unstable_v1();

        void init(struct ::wl_registry *registry, int id, int version);
        void init(struct ::zkde_screencast_stream_unstable_v1 *object);

        struct ::zkde_screencast_stream_unstable_v1 *object() { return m_zkde_screencast_stream_unstable_v1; }
        const struct ::zkde_screencast_stream_unstable_v1 *object() const { return m_zkde_screencast_stream_unstable_v1; }
        static zkde_screencast_stream_unstable_v1 *fromObject(struct ::zkde_screencast_stream_unstable_v1 *object);

        bool isInitialized() const;

        static const struct ::wl_interface *interface();

        void close();

    protected:
        virtual void zkde_screencast_stream_unstable_v1_closed();
        virtual void zkde_screencast_stream_unstable_v1_created(uint32_t node);
        virtual void zkde_screencast_stream_unstable_v1_failed(const QString &error);

    private:
        void init_listener();
        static const struct zkde_screencast_stream_unstable_v1_listener m_zkde_screencast_stream_unstable_v1_listener;
        static void handle_closed(
            void *data,
            struct ::zkde_screencast_stream_unstable_v1 *object);
        static void handle_created(
            void *data,
            struct ::zkde_screencast_stream_unstable_v1 *object,
            uint32_t node);
        static void handle_failed(
            void *data,
            struct ::zkde_screencast_stream_unstable_v1 *object,
            const char *error);
        struct ::zkde_screencast_stream_unstable_v1 *m_zkde_screencast_stream_unstable_v1;
    };
}

QT_WARNING_POP
QT_END_NAMESPACE

#endif
