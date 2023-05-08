// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef ZDDE_SCREENCAST_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define ZDDE_SCREENCAST_UNSTABLE_V1_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct wl_output;
struct zkde_screencast_stream_unstable_v1;
struct zkde_screencast_unstable_v1;

/**
 * @page page_iface_zkde_screencast_unstable_v1 zkde_screencast_unstable_v1
 * @section page_iface_zkde_screencast_unstable_v1_desc Description
 * @section page_iface_zkde_screencast_unstable_v1_api API
 * See @ref iface_zkde_screencast_unstable_v1.
 */
/**
 * @defgroup iface_zkde_screencast_unstable_v1 The zkde_screencast_unstable_v1 interface
 */
extern const struct wl_interface zkde_screencast_unstable_v1_interface;
/**
 * @page page_iface_zkde_screencast_stream_unstable_v1 zkde_screencast_stream_unstable_v1
 * @section page_iface_zkde_screencast_stream_unstable_v1_api API
 * See @ref iface_zkde_screencast_stream_unstable_v1.
 */
/**
 * @defgroup iface_zkde_screencast_stream_unstable_v1 The zkde_screencast_stream_unstable_v1 interface
 */
extern const struct wl_interface zkde_screencast_stream_unstable_v1_interface;

#ifndef ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_ENUM
#define ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_ENUM
/**
 * @ingroup iface_zkde_screencast_unstable_v1
 * Stream consumer attachment attributes
 */
enum zkde_screencast_unstable_v1_pointer {
	/**
	 * No cursor
	 */
	ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_HIDDEN = 1,
	/**
	 * Render the cursor on the stream
	 */
	ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_EMBEDDED = 2,
	/**
	 * Send metadata about where the cursor is through PipeWire
	 */
	ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_METADATA = 4,
};
#endif /* ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_ENUM */

#define ZKDE_SCREENCAST_UNSTABLE_V1_STREAM_OUTPUT 0
#define ZKDE_SCREENCAST_UNSTABLE_V1_STREAM_WINDOW 1
#define ZKDE_SCREENCAST_UNSTABLE_V1_DESTROY 2


/**
 * @ingroup iface_zkde_screencast_unstable_v1
 */
#define ZKDE_SCREENCAST_UNSTABLE_V1_STREAM_OUTPUT_SINCE_VERSION 1
/**
 * @ingroup iface_zkde_screencast_unstable_v1
 */
#define ZKDE_SCREENCAST_UNSTABLE_V1_STREAM_WINDOW_SINCE_VERSION 1
/**
 * @ingroup iface_zkde_screencast_unstable_v1
 */
#define ZKDE_SCREENCAST_UNSTABLE_V1_DESTROY_SINCE_VERSION 1

/** @ingroup iface_zkde_screencast_unstable_v1 */
static inline void
zkde_screencast_unstable_v1_set_user_data(struct zkde_screencast_unstable_v1 *zkde_screencast_unstable_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zkde_screencast_unstable_v1, user_data);
}

/** @ingroup iface_zkde_screencast_unstable_v1 */
static inline void *
zkde_screencast_unstable_v1_get_user_data(struct zkde_screencast_unstable_v1 *zkde_screencast_unstable_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zkde_screencast_unstable_v1);
}

static inline uint32_t
zkde_screencast_unstable_v1_get_version(struct zkde_screencast_unstable_v1 *zkde_screencast_unstable_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zkde_screencast_unstable_v1);
}

/**
 * @ingroup iface_zkde_screencast_unstable_v1
 */
static inline struct zkde_screencast_stream_unstable_v1 *
zkde_screencast_unstable_v1_stream_output(struct zkde_screencast_unstable_v1 *zkde_screencast_unstable_v1, struct wl_output *output, uint32_t pointer)
{
	struct wl_proxy *stream;

	stream = wl_proxy_marshal_constructor((struct wl_proxy *) zkde_screencast_unstable_v1,
			 ZKDE_SCREENCAST_UNSTABLE_V1_STREAM_OUTPUT, &zkde_screencast_stream_unstable_v1_interface, NULL, output, pointer);

	return (struct zkde_screencast_stream_unstable_v1 *) stream;
}

/**
 * @ingroup iface_zkde_screencast_unstable_v1
 */
static inline struct zkde_screencast_stream_unstable_v1 *
zkde_screencast_unstable_v1_stream_window(struct zkde_screencast_unstable_v1 *zkde_screencast_unstable_v1, const char *window_uuid, uint32_t pointer)
{
	struct wl_proxy *stream;

	stream = wl_proxy_marshal_constructor((struct wl_proxy *) zkde_screencast_unstable_v1,
			 ZKDE_SCREENCAST_UNSTABLE_V1_STREAM_WINDOW, &zkde_screencast_stream_unstable_v1_interface, NULL, window_uuid, pointer);

	return (struct zkde_screencast_stream_unstable_v1 *) stream;
}

/**
 * @ingroup iface_zkde_screencast_unstable_v1
 *
 * Destroy the zkde_screencast_unstable_v1 object.
 */
static inline void
zkde_screencast_unstable_v1_destroy(struct zkde_screencast_unstable_v1 *zkde_screencast_unstable_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zkde_screencast_unstable_v1,
			 ZKDE_SCREENCAST_UNSTABLE_V1_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) zkde_screencast_unstable_v1);
}

/**
 * @ingroup iface_zkde_screencast_stream_unstable_v1
 * @struct zkde_screencast_stream_unstable_v1_listener
 */
struct zkde_screencast_stream_unstable_v1_listener {
	/**
	 * Notifies that the server has stopped the stream. Clients should now call close.
	 *
	 * 
	 */
	void (*closed)(void *data,
		       struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1);
	/**
	 * Notifies about a pipewire feed being created
	 *
	 * 
	 * @param node node of the pipewire buffer
	 */
	void (*created)(void *data,
			struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1,
			uint32_t node);
	/**
	 * Offers an error message so the client knows the created event will not arrive, and the client should close the resource.
	 *
	 * 
	 * @param error A human readable translated error message.
	 */
	void (*failed)(void *data,
		       struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1,
		       const char *error);
};

/**
 * @ingroup iface_zkde_screencast_stream_unstable_v1
 */
static inline int
zkde_screencast_stream_unstable_v1_add_listener(struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1,
						const struct zkde_screencast_stream_unstable_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zkde_screencast_stream_unstable_v1,
				     (void (**)(void)) listener, data);
}

#define ZKDE_SCREENCAST_STREAM_UNSTABLE_V1_CLOSE 0

/**
 * @ingroup iface_zkde_screencast_stream_unstable_v1
 */
#define ZKDE_SCREENCAST_STREAM_UNSTABLE_V1_CLOSED_SINCE_VERSION 1
/**
 * @ingroup iface_zkde_screencast_stream_unstable_v1
 */
#define ZKDE_SCREENCAST_STREAM_UNSTABLE_V1_CREATED_SINCE_VERSION 1
/**
 * @ingroup iface_zkde_screencast_stream_unstable_v1
 */
#define ZKDE_SCREENCAST_STREAM_UNSTABLE_V1_FAILED_SINCE_VERSION 1

/**
 * @ingroup iface_zkde_screencast_stream_unstable_v1
 */
#define ZKDE_SCREENCAST_STREAM_UNSTABLE_V1_CLOSE_SINCE_VERSION 1

/** @ingroup iface_zkde_screencast_stream_unstable_v1 */
static inline void
zkde_screencast_stream_unstable_v1_set_user_data(struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zkde_screencast_stream_unstable_v1, user_data);
}

/** @ingroup iface_zkde_screencast_stream_unstable_v1 */
static inline void *
zkde_screencast_stream_unstable_v1_get_user_data(struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zkde_screencast_stream_unstable_v1);
}

static inline uint32_t
zkde_screencast_stream_unstable_v1_get_version(struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zkde_screencast_stream_unstable_v1);
}

/** @ingroup iface_zkde_screencast_stream_unstable_v1 */
static inline void
zkde_screencast_stream_unstable_v1_destroy(struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1)
{
	wl_proxy_destroy((struct wl_proxy *) zkde_screencast_stream_unstable_v1);
}

/**
 * @ingroup iface_zkde_screencast_stream_unstable_v1
 */
static inline void
zkde_screencast_stream_unstable_v1_close(struct zkde_screencast_stream_unstable_v1 *zkde_screencast_stream_unstable_v1)
{
	wl_proxy_marshal((struct wl_proxy *) zkde_screencast_stream_unstable_v1,
			 ZKDE_SCREENCAST_STREAM_UNSTABLE_V1_CLOSE);

	wl_proxy_destroy((struct wl_proxy *) zkde_screencast_stream_unstable_v1);
}

#ifdef  __cplusplus
}
#endif

#endif
