#define _GNU_SOURCE 1
#include <stddef.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

static ssize_t (*orig_read)(int fd, void* buf, size_t count) = NULL;
static ssize_t (*orig_write)(int fd, const void* buf, size_t count) = NULL;
static ssize_t (*orig_recv)(int fd, void* buf, size_t count, int flags) = NULL;
static ssize_t (*orig_send)(int fd, const void* buf, size_t count, int flags) = NULL;
static ssize_t (*orig_recvfrom)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) = NULL;
static ssize_t (*orig_sendto)(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *src_addr, socklen_t addrlen) = NULL;
static ssize_t (*orig_readv)(int fd, const struct iovec *iov, int iovcnt) = NULL;
static ssize_t (*orig_writev)(int fd, const struct iovec *iov, int iovcnt) = NULL;

#define INITIALIZED 1
#define TOUCH_READS 2
#define TOUCH_WRITES 4
#define TOUCH_V 8

static volatile unsigned int rnd = 0x1CE4E5B9;
static volatile int flags = 0;

// Source: https://en.wikipedia.org/w/index.php?title=Xorshift&oldid=989828061
static uint32_t xorshift32(volatile uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static void initialize() {
    if (!flags) {
        flags |= 1;
        if (getenv("LIBSHORTRECV_SEED")) {
            rnd = atoi(getenv("LIBSHORTRECV_SEED"));
        }
        if (!getenv("LIBSHORTRECV_NOREAD") || !atoi(getenv("LIBSHORTRECV_NOREAD"))) {
            flags |= TOUCH_READS;
        }
        if (!getenv("LIBSHORTRECV_NOWRITE") || !atoi(getenv("LIBSHORTRECV_NOWRITE"))) {
            flags |= TOUCH_WRITES;
        }
        if (!getenv("LIBSHORTRECV_NOV") || !atoi(getenv("LIBSHORTRECV_NOV"))) {
            flags |= TOUCH_V;
        }
    }
}
static void mangle_simple(size_t *count) {
    uint32_t r = xorshift32(&rnd);
    if (!*count) {
        return;
    }
    if (r < 0x80000000) {
        r &= 0xF;
    }
    *count = r % *count;
    if(!*count) {
        *count=1;
    }
}
static void mangle_simple2(int *count) {
    uint32_t r = xorshift32(&rnd);
    if (!*count) {
        return;
    }
    *count = r % *count;
    if(!*count) {
        *count=1;
    }
}

static void mangle_complex(const struct iovec *iov, int *iovcnt, size_t* backup)  {
    if (!*iovcnt) return;
    mangle_simple2(iovcnt);
    struct iovec *iov_patch = (struct iovec*) iov;
    *backup = iov_patch[*iovcnt-1].iov_len;
    mangle_simple(&iov_patch[*iovcnt-1].iov_len);
}
static void restore_complex(const struct iovec *iov, int *iovcnt, size_t* backup)  {
    if (!*iovcnt) return;
    struct iovec *iov_patch = (struct iovec*) iov;
    iov_patch[*iovcnt-1].iov_len = *backup;
}


ssize_t read(int fd, void* buf, size_t count) {
    if(!orig_read) {
        orig_read = dlsym(RTLD_NEXT, "read");
    }
    initialize();
    if (flags & TOUCH_READS) mangle_simple(&count);
    return orig_read(fd, buf, count);
}

ssize_t write(int fd, const void* buf, size_t count) {
    if(!orig_write) {
        orig_write = dlsym(RTLD_NEXT, "write");
    }
    initialize();
    if (flags & TOUCH_WRITES) mangle_simple(&count);
    return orig_write(fd, buf, count);
}
ssize_t recv(int fd, void* buf, size_t count, int flags) {
    if(!orig_recv) {
        orig_recv = dlsym(RTLD_NEXT, "recv");
    }
    initialize();
    if (flags & TOUCH_READS) mangle_simple(&count);
    return orig_recv(fd, buf, count, flags);
}

ssize_t send(int fd, const void* buf, size_t count, int flags) {
    if(!orig_send) {
        orig_send = dlsym(RTLD_NEXT, "send");
    }
    initialize();
    if (flags & TOUCH_WRITES) mangle_simple(&count);
    return orig_send(fd, buf, count, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    if(!orig_recvfrom) {
        orig_recvfrom = dlsym(RTLD_NEXT, "recvfrom");
    }
    initialize();
    if (flags & TOUCH_READS) mangle_simple(&len);
    return orig_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen) {
    if(!orig_sendto) {
        orig_sendto = dlsym(RTLD_NEXT, "sendto");
    }
    initialize();
    if (flags & TOUCH_WRITES) mangle_simple(&len);
    return orig_sendto(sockfd, buf, len, flags, addr, addrlen);
}


ssize_t readv (int fd, const struct iovec *iov, int iovcnt) {
    if(!orig_readv) {
        orig_readv = dlsym(RTLD_NEXT, "readv");
    }
    initialize();
    size_t backup;
    if ((flags & (TOUCH_READS|TOUCH_V)) == (TOUCH_READS|TOUCH_V)) mangle_complex(iov, &iovcnt, &backup);
    ssize_t ret = orig_readv(fd, iov, iovcnt);
    restore_complex(iov, &iovcnt, &backup);
    return ret;
}
ssize_t writev (int fd, const struct iovec *iov, int iovcnt) {
    if(!orig_writev) {
        orig_writev = dlsym(RTLD_NEXT, "writev");
    }
    initialize();
    size_t backup;
    if ((flags & (TOUCH_WRITES|TOUCH_V)) == (TOUCH_WRITES|TOUCH_V)) mangle_complex(iov, &iovcnt, &backup);
    ssize_t ret = orig_writev(fd, iov, iovcnt);
    restore_complex(iov, &iovcnt, &backup);
    return ret;
}
