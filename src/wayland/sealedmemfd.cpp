// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "sealedmemfd.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace ScreenCastMemory {

int createSealedMemFd(const char *name, uint32_t size)
{
    if (!name || size == 0) {
        errno = EINVAL;
        return -1;
    }

    const int fd = memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        return -1;
    }

    if (ftruncate(fd, static_cast<off_t>(size)) < 0
        || fcntl(fd,
                 F_ADD_SEALS,
                 F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0) {
        const int savedErrno = errno;
        close(fd);
        errno = savedErrno;
        return -1;
    }

    return fd;
}

}
