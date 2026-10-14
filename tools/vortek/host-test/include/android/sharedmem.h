/* Host-test stub (tools/vortek/host-test): ASharedMemory as a memfd. */
#pragma once
#include <sys/syscall.h>
#include <unistd.h>
static inline int ASharedMemory_create(const char* name, size_t size) {
    int fd = (int)syscall(SYS_memfd_create, name, 0);
    if (fd >= 0 && ftruncate(fd, size) != 0) { close(fd); return -1; }
    return fd;
}
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 2U
#endif
