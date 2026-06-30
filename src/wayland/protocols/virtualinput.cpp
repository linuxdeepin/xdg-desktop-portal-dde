// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "virtualinput.h"
#include "common.h"
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <cstring>
#include <cstdlib>

VirtualPointerManager::VirtualPointerManager() : QWaylandClientExtensionTemplate(2) { initialize(); }
VirtualKeyboardManager::VirtualKeyboardManager() : QWaylandClientExtensionTemplate(1) { initialize(); }
TransientSeatManager::TransientSeatManager() : QWaylandClientExtensionTemplate(1) { initialize(); }

bool TransientSeat::create(TransientSeatManager &manager)
{
    reset();
    if (!manager.isActive())
        return false;

    const auto display = waylandDisplay();
    if (!display)
        return false;
    m_registry = wl_display_get_registry(display->wl_display());
    init(manager.create());
    if (!isInitialized())
        return false;

    wl_display_roundtrip(display->wl_display());
    return m_seat && !m_denied;
}

void TransientSeat::reset()
{
    if (m_seat) {
        wl_seat_destroy(m_seat);
        m_seat = nullptr;
    }
    if (isInitialized())
        destroy();
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    m_denied = false;
}

void TransientSeat::ext_transient_seat_v1_ready(uint32_t globalName)
{
    m_seat = static_cast<wl_seat *>(wl_registry_bind(m_registry, globalName, &wl_seat_interface, 1));
}

void TransientSeat::ext_transient_seat_v1_denied()
{
    m_denied = true;
}

VirtualInput::VirtualInput() { m_clock.start(); }
VirtualInput::~VirtualInput()
{
    if (m_pointer.isInitialized()) m_pointer.destroy();
    if (m_keyboard.isInitialized()) m_keyboard.destroy();
    if (m_xkbKeymap) xkb_keymap_unref(m_xkbKeymap);
    if (m_xkbContext) xkb_context_unref(m_xkbContext);
    m_transientSeat.reset();
}
bool VirtualInput::initialize(PortalCommon::DeviceTypes devices)
{
    return m_transientSeat.create(m_transientSeatManager)
        && (!devices.testFlag(PortalCommon::Pointer) || m_pointerManager.isActive())
        && (!devices.testFlag(PortalCommon::Keyboard) || m_keyboardManager.isActive());
}
uint32_t VirtualInput::timestamp() const { return static_cast<uint32_t>(m_clock.elapsed()); }
void VirtualInput::ensurePointer()
{
    if (!m_pointer.isInitialized() && m_pointerManager.isActive() && m_transientSeat.seat())
        m_pointer.init(m_pointerManager.create_virtual_pointer(m_transientSeat.seat()));
}
void VirtualInput::ensureKeyboard()
{
    if (m_keyboard.isInitialized() || !m_keyboardManager.isActive()) return;
    if (!m_transientSeat.seat()) return;
    m_keyboard.init(m_keyboardManager.create_virtual_keyboard(m_transientSeat.seat()));
    m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    m_xkbKeymap = m_xkbContext ? xkb_keymap_new_from_names(m_xkbContext, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS) : nullptr;
    char *keymap = m_xkbKeymap ? xkb_keymap_get_as_string(m_xkbKeymap, XKB_KEYMAP_FORMAT_TEXT_V1) : nullptr;
    if (!keymap) return;
    const auto size = static_cast<uint32_t>(std::strlen(keymap) + 1);
    if (m_keymapFile.open() && m_keymapFile.write(keymap, size) == size) {
        m_keymapFile.flush();
        m_keyboard.keymap(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, m_keymapFile.handle(), size);
    }
    std::free(keymap);
}
bool VirtualInput::pointerAvailable() { ensurePointer(); return m_pointer.isInitialized(); }
bool VirtualInput::keyboardAvailable() { ensureKeyboard(); return m_keyboard.isInitialized(); }
void VirtualInput::pointerMotion(double dx, double dy)
{
    if (!pointerAvailable()) return;
    m_pointer.motion(timestamp(), wl_fixed_from_double(dx), wl_fixed_from_double(dy)); m_pointer.frame();
}
void VirtualInput::pointerMotionAbsolute(double x, double y, uint32_t width, uint32_t height)
{
    if (!pointerAvailable() || !width || !height) return;
    m_pointer.motion_absolute(timestamp(), qBound(0u, static_cast<uint32_t>(qMax(0.0, x)), width), qBound(0u, static_cast<uint32_t>(qMax(0.0, y)), height), width, height); m_pointer.frame();
}
void VirtualInput::pointerButton(uint32_t button, bool pressed)
{
    if (!pointerAvailable()) return;
    m_pointer.button(timestamp(), button, pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED); m_pointer.frame();
}
void VirtualInput::pointerAxis(double dx, double dy)
{
    if (!pointerAvailable()) return;
    if (dy) m_pointer.axis(timestamp(), WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_double(dy));
    if (dx) m_pointer.axis(timestamp(), WL_POINTER_AXIS_HORIZONTAL_SCROLL, wl_fixed_from_double(dx));
    m_pointer.frame();
}
void VirtualInput::pointerAxisDiscrete(uint32_t axis, int32_t steps)
{
    if (!pointerAvailable()) return;
    const uint32_t wlAxis = axis == 0 ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL;
    m_pointer.axis_discrete(timestamp(), wlAxis, wl_fixed_from_int(steps * 10), steps); m_pointer.frame();
}
void VirtualInput::keyboardKeycode(uint32_t keycode, bool pressed)
{
    if (keyboardAvailable()) m_keyboard.key(timestamp(), keycode, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
}
bool VirtualInput::keyboardKeysym(uint32_t keysym, bool pressed)
{
    if (!keyboardAvailable() || !m_xkbKeymap) return false;
    for (xkb_keycode_t code = xkb_keymap_min_keycode(m_xkbKeymap); code <= xkb_keymap_max_keycode(m_xkbKeymap); ++code) {
        const xkb_keysym_t *syms = nullptr;
        const int count = xkb_keymap_key_get_syms_by_level(m_xkbKeymap, code, 0, 0, &syms);
        for (int i = 0; i < count; ++i) if (syms[i] == keysym) { keyboardKeycode(code - 8, pressed); return true; }
    }
    return false;
}
