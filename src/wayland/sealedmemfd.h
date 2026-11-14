// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>

namespace ScreenCastMemory {

// Creates a writable memfd whose size can no longer be changed. The caller
// owns the returned descriptor.
int createSealedMemFd(const char *name, uint32_t size);

}
