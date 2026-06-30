// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qwayland-ext-transient-seat-v1.h"
#include "qwayland-virtual-keyboard-unstable-v1.h"
#include "qwayland-wlr-virtual-pointer-unstable-v1.h"
#include "portalcommon.h"
#include <QElapsedTimer>
#include <QTemporaryFile>
#include <QtWaylandClient/QWaylandClientExtension>

struct xkb_context;
struct xkb_keymap;
struct wl_registry;
struct wl_seat;

class VirtualPointerManager final : public QWaylandClientExtensionTemplate<VirtualPointerManager>, public QtWayland::zwlr_virtual_pointer_manager_v1
{
public:
    VirtualPointerManager();
};

class VirtualKeyboardManager final : public QWaylandClientExtensionTemplate<VirtualKeyboardManager>, public QtWayland::zwp_virtual_keyboard_manager_v1
{
public:
    VirtualKeyboardManager();
};

class TransientSeatManager final : public QWaylandClientExtensionTemplate<TransientSeatManager>, public QtWayland::ext_transient_seat_manager_v1
{
public:
    TransientSeatManager();
};

class TransientSeat final : public QtWayland::ext_transient_seat_v1
{
public:
    bool create(TransientSeatManager &manager);
    void reset();
    struct wl_seat *seat() const { return m_seat; }

protected:
    void ext_transient_seat_v1_ready(uint32_t globalName) override;
    void ext_transient_seat_v1_denied() override;

private:
    struct wl_registry *m_registry = nullptr;
    struct wl_seat *m_seat = nullptr;
    bool m_denied = false;
};

class VirtualInput final
{
public:
    VirtualInput();
    ~VirtualInput();
    bool initialize(PortalCommon::DeviceTypes devices);
    bool pointerAvailable();
    bool keyboardAvailable();
    void pointerMotion(double dx, double dy);
    void pointerMotionAbsolute(double x, double y, uint32_t width, uint32_t height);
    void pointerButton(uint32_t button, bool pressed);
    void pointerAxis(double dx, double dy);
    void pointerAxisDiscrete(uint32_t axis, int32_t steps);
    void keyboardKeycode(uint32_t keycode, bool pressed);
    bool keyboardKeysym(uint32_t keysym, bool pressed);

private:
    uint32_t timestamp() const;
    void ensurePointer();
    void ensureKeyboard();
    TransientSeatManager m_transientSeatManager;
    TransientSeat m_transientSeat;
    VirtualPointerManager m_pointerManager;
    VirtualKeyboardManager m_keyboardManager;
    QtWayland::zwlr_virtual_pointer_v1 m_pointer;
    QtWayland::zwp_virtual_keyboard_v1 m_keyboard;
    QElapsedTimer m_clock;
    QTemporaryFile m_keymapFile;
    xkb_context *m_xkbContext = nullptr;
    xkb_keymap *m_xkbKeymap = nullptr;
};
