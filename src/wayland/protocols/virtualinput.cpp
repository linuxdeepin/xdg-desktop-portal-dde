// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "virtualinput.h"
#include "common.h"
#include "loggings.h"
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <QScreen>
#include <QGuiApplication>
#include <private/qwaylandinputdevice_p.h>
#include <private/qwaylandscreen_p.h>

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
    if (!isInitialized()) {
        reset();
        return false;
    }

    if (wl_display_roundtrip(display->wl_display()) < 0) {
        qCWarning(REMOTEDESKTOP) << "Wayland roundtrip failed while creating a transient seat";
        reset();
        return false;
    }
    if (!m_seat || m_denied) {
        qCWarning(REMOTEDESKTOP) << "Transient seat creation failed:" << "denied=" << m_denied;
        reset();
        return false;
    }
    return true;
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
    if (m_pointer.isInitialized()) {
        for (uint32_t button : m_pressedButtons)
            m_pointer.button(timestamp(), button, WL_POINTER_BUTTON_STATE_RELEASED);
        m_pointer.frame();
        m_pointer.destroy();
    }
    if (m_keyboard.isInitialized()) {
        for (uint32_t key : m_pressedKeys)
            m_keyboard.key(timestamp(), key, WL_KEYBOARD_KEY_STATE_RELEASED);
        m_keyboard.modifiers(0, 0, 0, 0);
        m_keyboard.destroy();
    }
    if (m_xkbState) xkb_state_unref(m_xkbState);
    if (m_xkbKeymap) xkb_keymap_unref(m_xkbKeymap);
    if (m_xkbContext) xkb_context_unref(m_xkbContext);
    m_transientSeat.reset();
}
bool VirtualInput::initialize(PortalCommon::DeviceTypes devices)
{
    const auto display = waylandDisplay();
    if (!display) {
        qCWarning(REMOTEDESKTOP) << "Cannot initialize remote input without a Wayland display";
        return false;
    }
    if ((devices.testFlag(PortalCommon::Pointer) && !m_pointerManager.isActive())
        || (devices.testFlag(PortalCommon::Keyboard) && !m_keyboardManager.isActive())) {
        qCWarning(REMOTEDESKTOP) << "Requested virtual input protocols are unavailable:"
                               << "devices=" << devices
                               << "pointer=" << m_pointerManager.isActive()
                               << "keyboard=" << m_keyboardManager.isActive();
        return false;
    }

    if (m_transientSeatManager.isActive()) {
        // An explicit denial must not be bypassed by using the desktop seat.
        if (!m_transientSeat.create(m_transientSeatManager))
            return false;
        m_seat = m_transientSeat.seat();
        m_usingTransientSeat = true;
        qCInfo(REMOTEDESKTOP) << "Using transient seat for remote input";
    } else {
        // Prefer the desktop seat used to interact with the consent dialog.
        auto *device = display->lastInputDevice();
        if (!device || !(device->capabilities() & (WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD))) {
            device = nullptr;
            for (auto *candidate : display->inputDevices()) {
                if (candidate->capabilities() & (WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD)) {
                    device = candidate;
                    break;
                }
            }
        }
        if (!device) {
            qCWarning(REMOTEDESKTOP) << "No desktop seat is available for remote input";
            return false;
        }
        m_inputDevice = device;
        m_seat = device->wl_seat();
        qCInfo(REMOTEDESKTOP) << "Transient seat protocol unavailable; using desktop seat" << device->seatname()
                            << "global=" << device->id();
    }
    const bool ready = m_seat
        && (!devices.testFlag(PortalCommon::Pointer) || pointerAvailable())
        && (!devices.testFlag(PortalCommon::Keyboard) || keyboardAvailable());
    if (!ready) {
        qCWarning(REMOTEDESKTOP) << "Failed to initialize virtual input:"
                               << "transientSeat=" << m_usingTransientSeat << "devices=" << devices;
        return false;
    }
    qCInfo(REMOTEDESKTOP) << "Remote input initialized:" << "transientSeat=" << m_usingTransientSeat
                        << "devices=" << devices;
    display->flushRequests();
    return true;
}
uint32_t VirtualInput::timestamp() const { return static_cast<uint32_t>(m_clock.elapsed()); }
void VirtualInput::ensurePointer()
{
    if (!m_pointer.isInitialized() && m_pointerManager.isActive() && m_seat)
        m_pointer.init(m_pointerManager.create_virtual_pointer(m_seat));
}
void VirtualInput::ensureKeyboard()
{
    if (m_keyboard.isInitialized() || !m_keyboardManager.isActive() || !m_seat)
        return;
    if (!m_xkbContext)
        m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!m_xkbKeymap && m_xkbContext)
        m_xkbKeymap = xkb_keymap_new_from_names(m_xkbContext, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!m_xkbState && m_xkbKeymap)
        m_xkbState = xkb_state_new(m_xkbKeymap);
    char *keymap = m_xkbState ? xkb_keymap_get_as_string(m_xkbKeymap, XKB_KEYMAP_FORMAT_TEXT_V1) : nullptr;
    if (!keymap)
        return;
    const auto size = static_cast<uint32_t>(std::strlen(keymap) + 1);
    if (m_keymapFile.isOpen())
        m_keymapFile.resize(0);
    const bool written = (m_keymapFile.isOpen() || m_keymapFile.open())
        && m_keymapFile.seek(0) && m_keymapFile.write(keymap, size) == size && m_keymapFile.flush();
    std::free(keymap);
    if (!written)
        return;
    m_keyboard.init(m_keyboardManager.create_virtual_keyboard(m_seat));
    m_keyboard.keymap(WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, m_keymapFile.handle(), size);
    sendModifiers();
}
void VirtualInput::sendModifiers()
{
    uint32_t depressed = xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_DEPRESSED);
    for (const auto &symbol : m_symbolPresses)
        depressed |= symbol.modifiers;
    m_keyboard.modifiers(depressed,
                         xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_LATCHED),
                         xkb_state_serialize_mods(m_xkbState, XKB_STATE_MODS_LOCKED),
                         xkb_state_serialize_layout(m_xkbState, XKB_STATE_LAYOUT_EFFECTIVE));
}

bool VirtualInput::seatAvailable() const
{
    return m_usingTransientSeat ? m_transientSeat.seat() != nullptr : !m_inputDevice.isNull();
}

bool VirtualInput::pointerAvailable()
{
    if (!seatAvailable() || !m_pointerManager.isActive())
        return false;
    ensurePointer();
    return m_pointer.isInitialized();
}
bool VirtualInput::keyboardAvailable()
{
    if (!seatAvailable() || !m_keyboardManager.isActive())
        return false;
    ensureKeyboard();
    return m_keyboard.isInitialized();
}
void VirtualInput::pointerMotion(double dx, double dy)
{
    if (!std::isfinite(dx) || !std::isfinite(dy) || std::abs(dx) > 8388607 || std::abs(dy) > 8388607 || !pointerAvailable()) return;
    m_pointer.motion(timestamp(), wl_fixed_from_double(dx), wl_fixed_from_double(dy)); m_pointer.frame();
}
void VirtualInput::pointerMotionAbsolute(double x, double y, uint32_t width, uint32_t height, QScreen *output)
{
    if (!seatAvailable() || !m_pointerManager.isActive()
        || !std::isfinite(x) || !std::isfinite(y) || !output || !width || !height)
        return;
    // Version 1 maps absolute motion over the whole output layout.
    if (m_pointerManager.QtWayland::zwlr_virtual_pointer_manager_v1::version() < 2) {
        QRect layout;
        for (auto *screen : QGuiApplication::screens())
            layout = layout.united(screen->geometry());
        if (layout.isEmpty() || !pointerAvailable())
            return;
        const QPoint origin = output->geometry().topLeft() - layout.topLeft();
        m_pointer.motion_absolute(timestamp(),
                                  static_cast<uint32_t>(qBound(0.0, origin.x() + qBound(0.0, x, double(width)), double(layout.width()))),
                                  static_cast<uint32_t>(qBound(0.0, origin.y() + qBound(0.0, y, double(height)), double(layout.height()))),
                                  layout.width(), layout.height());
        m_pointer.frame();
        return;
    }
    // Version 2 can constrain the virtual pointer to the selected output.
    if (m_pointerOutput != output) {
        if (m_pointer.isInitialized()) {
            for (uint32_t button : m_pressedButtons)
                m_pointer.button(timestamp(), button, WL_POINTER_BUTTON_STATE_RELEASED);
            m_pointer.frame();
            m_pointer.destroy();
        }
        auto *screen = dynamic_cast<QtWaylandClient::QWaylandScreen *>(output->handle());
        if (!screen || !m_seat)
            return;
        m_pointer.init(m_pointerManager.create_virtual_pointer_with_output(m_seat, screen->output()));
        m_pointerOutput = output;
        for (uint32_t button : m_pressedButtons)
            m_pointer.button(timestamp(), button, WL_POINTER_BUTTON_STATE_PRESSED);
    }
    if (!pointerAvailable())
        return;
    m_pointer.motion_absolute(timestamp(), static_cast<uint32_t>(qBound(0.0, x, double(width))),
                              static_cast<uint32_t>(qBound(0.0, y, double(height))), width, height);
    m_pointer.frame();
}
void VirtualInput::pointerButton(uint32_t button, bool pressed)
{
    if (!pointerAvailable()) return;
    if (m_pressedButtons.contains(button) == pressed)
        return;
    if (pressed)
        m_pressedButtons.insert(button);
    else
        m_pressedButtons.remove(button);
    m_pointer.button(timestamp(), button, pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
    m_pointer.frame();
}
void VirtualInput::pointerAxis(double dx, double dy, bool finish)
{
    if (!std::isfinite(dx) || !std::isfinite(dy) || std::abs(dx) > 8388607 || std::abs(dy) > 8388607 || !pointerAvailable()) return;
    m_pointer.axis_source(WL_POINTER_AXIS_SOURCE_FINGER);
    if (dy) m_pointer.axis(timestamp(), WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_double(dy));
    if (dx) m_pointer.axis(timestamp(), WL_POINTER_AXIS_HORIZONTAL_SCROLL, wl_fixed_from_double(dx));
    if (finish) {
        m_pointer.axis_stop(timestamp(), WL_POINTER_AXIS_VERTICAL_SCROLL);
        m_pointer.axis_stop(timestamp(), WL_POINTER_AXIS_HORIZONTAL_SCROLL);
    }
    m_pointer.frame();
}
void VirtualInput::pointerAxisDiscrete(uint32_t axis, int32_t steps)
{
    if (axis > 1 || steps > 838860 || steps < -838860 || !pointerAvailable()) return;
    m_pointer.axis_source(WL_POINTER_AXIS_SOURCE_WHEEL);
    const uint32_t wlAxis = axis == 0 ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL;
    m_pointer.axis_discrete(timestamp(), wlAxis, wl_fixed_from_int(steps * 10), steps); m_pointer.frame();
}
void VirtualInput::keyboardKeycode(uint32_t keycode, bool pressed)
{
    if (!keyboardAvailable() || keycode > xkb_keymap_max_keycode(m_xkbKeymap) - 8)
        return;
    if (m_pressedKeys.contains(keycode) == pressed)
        return;
    if (pressed)
        m_pressedKeys.insert(keycode);
    else
        m_pressedKeys.remove(keycode);
    m_keyboard.key(timestamp(), keycode, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
    xkb_state_update_key(m_xkbState, keycode + 8, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
    sendModifiers();
}
bool VirtualInput::keyboardKeysym(uint32_t keysym, bool pressed)
{
    if (!keyboardAvailable())
        return false;
    auto it = m_symbolPresses.find(keysym);
    if (!pressed) {
        if (it == m_symbolPresses.end())
            return false;
        const uint32_t code = it->keycode;
        keyboardKeycode(code, false);
        m_symbolPresses.erase(it);
        sendModifiers();
        return true;
    }
    if (it != m_symbolPresses.end())
        return true;
    const xkb_layout_index_t layout = xkb_state_serialize_layout(m_xkbState, XKB_STATE_LAYOUT_EFFECTIVE);
    for (xkb_keycode_t code = xkb_keymap_min_keycode(m_xkbKeymap); code <= xkb_keymap_max_keycode(m_xkbKeymap); ++code) {
        if (code < 8)
            continue;
        for (xkb_level_index_t level = 0; level < xkb_keymap_num_levels_for_key(m_xkbKeymap, code, layout); ++level) {
            const xkb_keysym_t *syms = nullptr;
            const int count = xkb_keymap_key_get_syms_by_level(m_xkbKeymap, code, layout, level, &syms);
            if (count != 1 || syms[0] != keysym)
                continue;
            xkb_mod_mask_t modifiers = 0;
            if (level && !xkb_keymap_key_get_mods_for_level(m_xkbKeymap, code, layout, level, &modifiers, 1))
                continue;
            m_symbolPresses.insert(keysym, SymbolPress{code - 8, modifiers});
            sendModifiers();
            keyboardKeycode(code - 8, true);
            return true;
        }
    }
    return false;
}
