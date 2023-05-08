// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qwayland-zkde-screencast-unstable-v1.h"

QT_BEGIN_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wmissing-field-initializers")

namespace QtWayland {

static inline void *wlRegistryBind(struct ::wl_registry *registry, uint32_t name, const struct ::wl_interface *interface, uint32_t version)
{
    const uint32_t bindOpCode = 0;
#if (WAYLAND_VERSION_MAJOR == 1 && WAYLAND_VERSION_MINOR > 10) || WAYLAND_VERSION_MAJOR > 1
    return (void *) wl_proxy_marshal_constructor_versioned((struct wl_proxy *) registry,
        bindOpCode, interface, version, name, interface->name, version, nullptr);
#else
    return (void *) wl_proxy_marshal_constructor((struct wl_proxy *) registry,
        bindOpCode, interface, name, interface->name, version, nullptr);
#endif
}

    zkde_screencast_unstable_v1::zkde_screencast_unstable_v1(struct ::wl_registry *registry, int id, int version)
    {
        init(registry, id, version);
    }

    zkde_screencast_unstable_v1::zkde_screencast_unstable_v1(struct ::zkde_screencast_unstable_v1 *obj)
        : m_zkde_screencast_unstable_v1(obj)
    {
    }

    zkde_screencast_unstable_v1::zkde_screencast_unstable_v1()
        : m_zkde_screencast_unstable_v1(nullptr)
    {
    }

    zkde_screencast_unstable_v1::~zkde_screencast_unstable_v1()
    {
    }

    void zkde_screencast_unstable_v1::init(struct ::wl_registry *registry, int id, int version)
    {
        m_zkde_screencast_unstable_v1 = static_cast<struct ::zkde_screencast_unstable_v1 *>(wlRegistryBind(registry, id, &zkde_screencast_unstable_v1_interface, version));
    }

    void zkde_screencast_unstable_v1::init(struct ::zkde_screencast_unstable_v1 *obj)
    {
        m_zkde_screencast_unstable_v1 = obj;
    }

    zkde_screencast_unstable_v1 *zkde_screencast_unstable_v1::fromObject(struct ::zkde_screencast_unstable_v1 *object)
    {
        return static_cast<zkde_screencast_unstable_v1 *>(zkde_screencast_unstable_v1_get_user_data(object));
    }

    bool zkde_screencast_unstable_v1::isInitialized() const
    {
        return m_zkde_screencast_unstable_v1 != nullptr;
    }

    const struct wl_interface *zkde_screencast_unstable_v1::interface()
    {
        return &::zkde_screencast_unstable_v1_interface;
    }

    struct ::zkde_screencast_stream_unstable_v1 *zkde_screencast_unstable_v1::stream_output(struct ::wl_output *output, uint32_t pointer)
    {
        return zkde_screencast_unstable_v1_stream_output(
            m_zkde_screencast_unstable_v1,
            output,
            pointer);
    }

    struct ::zkde_screencast_stream_unstable_v1 *zkde_screencast_unstable_v1::stream_window(const QString &window_uuid, uint32_t pointer)
    {
        return zkde_screencast_unstable_v1_stream_window(
            m_zkde_screencast_unstable_v1,
            window_uuid.toUtf8().constData(),
            pointer);
    }

    void zkde_screencast_unstable_v1::destroy()
    {
        zkde_screencast_unstable_v1_destroy(
            m_zkde_screencast_unstable_v1);
        m_zkde_screencast_unstable_v1 = nullptr;
    }

    zkde_screencast_stream_unstable_v1::zkde_screencast_stream_unstable_v1(struct ::wl_registry *registry, int id, int version)
    {
        init(registry, id, version);
    }

    zkde_screencast_stream_unstable_v1::zkde_screencast_stream_unstable_v1(struct ::zkde_screencast_stream_unstable_v1 *obj)
        : m_zkde_screencast_stream_unstable_v1(obj)
    {
        init_listener();
    }

    zkde_screencast_stream_unstable_v1::zkde_screencast_stream_unstable_v1()
        : m_zkde_screencast_stream_unstable_v1(nullptr)
    {
    }

    zkde_screencast_stream_unstable_v1::~zkde_screencast_stream_unstable_v1()
    {
    }

    void zkde_screencast_stream_unstable_v1::init(struct ::wl_registry *registry, int id, int version)
    {
        m_zkde_screencast_stream_unstable_v1 = static_cast<struct ::zkde_screencast_stream_unstable_v1 *>(wlRegistryBind(registry, id, &zkde_screencast_stream_unstable_v1_interface, version));
        init_listener();
    }

    void zkde_screencast_stream_unstable_v1::init(struct ::zkde_screencast_stream_unstable_v1 *obj)
    {
        m_zkde_screencast_stream_unstable_v1 = obj;
        init_listener();
    }

    zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1::fromObject(struct ::zkde_screencast_stream_unstable_v1 *object)
    {
        if (wl_proxy_get_listener((struct ::wl_proxy *)object) != (void *)&m_zkde_screencast_stream_unstable_v1_listener)
            return nullptr;
        return static_cast<zkde_screencast_stream_unstable_v1 *>(zkde_screencast_stream_unstable_v1_get_user_data(object));
    }

    bool zkde_screencast_stream_unstable_v1::isInitialized() const
    {
        return m_zkde_screencast_stream_unstable_v1 != nullptr;
    }

    const struct wl_interface *zkde_screencast_stream_unstable_v1::interface()
    {
        return &::zkde_screencast_stream_unstable_v1_interface;
    }

    void zkde_screencast_stream_unstable_v1::close()
    {
        zkde_screencast_stream_unstable_v1_close(
            m_zkde_screencast_stream_unstable_v1);
        m_zkde_screencast_stream_unstable_v1 = nullptr;
    }

    void zkde_screencast_stream_unstable_v1::zkde_screencast_stream_unstable_v1_closed()
    {
    }

    void zkde_screencast_stream_unstable_v1::handle_closed(
        void *data,
        struct ::zkde_screencast_stream_unstable_v1 *object)
    {
        Q_UNUSED(object);
        static_cast<zkde_screencast_stream_unstable_v1 *>(data)->zkde_screencast_stream_unstable_v1_closed();
    }

    void zkde_screencast_stream_unstable_v1::zkde_screencast_stream_unstable_v1_created(uint32_t )
    {
    }

    void zkde_screencast_stream_unstable_v1::handle_created(
        void *data,
        struct ::zkde_screencast_stream_unstable_v1 *object,
        uint32_t node)
    {
        Q_UNUSED(object);
        static_cast<zkde_screencast_stream_unstable_v1 *>(data)->zkde_screencast_stream_unstable_v1_created(
            node);
    }

    void zkde_screencast_stream_unstable_v1::zkde_screencast_stream_unstable_v1_failed(const QString &)
    {
    }

    void zkde_screencast_stream_unstable_v1::handle_failed(
        void *data,
        struct ::zkde_screencast_stream_unstable_v1 *object,
        const char *error)
    {
        Q_UNUSED(object);
        static_cast<zkde_screencast_stream_unstable_v1 *>(data)->zkde_screencast_stream_unstable_v1_failed(
            QString::fromUtf8(error));
    }

    const struct zkde_screencast_stream_unstable_v1_listener zkde_screencast_stream_unstable_v1::m_zkde_screencast_stream_unstable_v1_listener = {
        zkde_screencast_stream_unstable_v1::handle_closed,
        zkde_screencast_stream_unstable_v1::handle_created,
        zkde_screencast_stream_unstable_v1::handle_failed,
    };

    void zkde_screencast_stream_unstable_v1::init_listener()
    {
        zkde_screencast_stream_unstable_v1_add_listener(m_zkde_screencast_stream_unstable_v1, &m_zkde_screencast_stream_unstable_v1_listener, this);
    }
}

QT_WARNING_POP
QT_END_NAMESPACE
