// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qwayland-ext-transient-seat-v1.h"
#include "qwayland-virtual-keyboard-unstable-v1.h"
#include "qwayland-wlr-virtual-pointer-unstable-v1.h"
#include "portalcommon.h"
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <QTemporaryFile>
#include <QtWaylandClient/QWaylandClientExtension>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;
class QScreen;
struct wl_registry;
struct wl_seat;
namespace QtWaylandClient { class QWaylandInputDevice; }

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
    void pointerMotionAbsolute(double x, double y, uint32_t width, uint32_t height, QScreen *output);
    void pointerButton(uint32_t button, bool pressed);
    void pointerAxis(double dx, double dy, bool finish = false);
    void pointerAxisDiscrete(uint32_t axis, int32_t steps);
    void keyboardKeycode(uint32_t keycode, bool pressed);
    bool keyboardKeysym(uint32_t keysym, bool pressed);

private:
    uint32_t timestamp() const;
    void ensurePointer();
    void ensureKeyboard();
    void sendModifiers();
    bool seatAvailable() const;
    wl_seat *m_seat = nullptr;
    QPointer<QScreen> m_pointerOutput;
    struct SymbolPress { uint32_t keycode; uint32_t modifiers; };
    QHash<uint32_t, SymbolPress> m_symbolPresses;
    QSet<uint32_t> m_pressedKeys;
    QSet<uint32_t> m_pressedButtons;
    xkb_state *m_xkbState = nullptr;
    QPointer<QtWaylandClient::QWaylandInputDevice> m_inputDevice;
    bool m_usingTransientSeat = false;
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
