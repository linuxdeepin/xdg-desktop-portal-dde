// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wayland/sealedmemfd.h"

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

bool expect(bool condition, const char *message)
{
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

}

int main()
{
    constexpr uint32_t bufferSize = 4096;
    const int fd =
            ScreenCastMemory::createSealedMemFd("xdpw-dde-test", bufferSize);
    if (!expect(fd >= 0, "Failed to create a sealed capture memfd")) {
        return 1;
    }

    struct stat info = {};
    const int descriptorFlags = fcntl(fd, F_GETFD);
    const int seals = fcntl(fd, F_GET_SEALS);
    bool ok = expect(fstat(fd, &info) == 0, "Failed to stat the capture memfd")
            && expect(info.st_size == bufferSize,
                      "The capture memfd has an unexpected size")
            && expect(descriptorFlags >= 0
                              && (descriptorFlags & FD_CLOEXEC) != 0,
                      "The capture memfd is not close-on-exec")
            && expect(seals >= 0
                              && (seals & F_SEAL_GROW) != 0
                              && (seals & F_SEAL_SHRINK) != 0
                              && (seals & F_SEAL_SEAL) != 0
                              && (seals & F_SEAL_WRITE) == 0,
                      "The capture memfd has incorrect seals");

    errno = 0;
    ok = expect(ftruncate(fd, bufferSize * 2U) < 0 && errno == EPERM,
                "The capture memfd can still be grown")
            && ok;
    errno = 0;
    ok = expect(ftruncate(fd, bufferSize / 2U) < 0 && errno == EPERM,
                "The capture memfd can still be shrunk")
            && ok;
    errno = 0;
    ok = expect(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) < 0 && errno == EPERM,
                "Another process can still make the capture memfd read-only")
            && ok;

    void *mapping = mmap(nullptr,
                         bufferSize,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         fd,
                         0);
    ok = expect(mapping != MAP_FAILED,
                "The compositor cannot create a writable mapping")
            && ok;
    if (mapping != MAP_FAILED) {
        static_cast<uint8_t *>(mapping)[0] = 0x5a;
        ok = expect(msync(mapping, bufferSize, MS_SYNC) == 0,
                    "Failed to synchronize the writable mapping")
                && ok;
        munmap(mapping, bufferSize);
    }

    close(fd);
    return ok ? 0 : 1;
}
