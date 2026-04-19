/*
 * macOS compatibility shim for POSIX message queues.
 *
 * macOS removed POSIX mqueue support. This header emulates the subset used by
 * avatar_posix.c via named FIFOs with a 4-byte little-endian length prefix so
 * message boundaries are preserved.
 *
 * All messages in avatar are small fixed-size structs (< 64 bytes), well under
 * PIPE_BUF on both macOS and Linux, so header+payload writes are atomic.
 *
 * Opening uses O_RDWR on both ends to avoid blocking until the peer connects.
 */

#ifndef MACOS_MQUEUE_H
#define MACOS_MQUEUE_H

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

typedef int mqd_t;
#define MQD_INVALID (-1)

struct mq_attr {
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
};

static inline void _mq_to_path(const char *name, char *path, size_t len)
{
    snprintf(path, len, "/tmp/mq_%s", (name[0] == '/') ? name + 1 : name);
}

/*
 * mq_open: create the FIFO when O_CREAT is set, then open it O_RDWR so we
 * don't block waiting for the peer.
 */
static inline mqd_t mq_open(const char *name, int oflag, ...)
{
    char path[256];
    _mq_to_path(name, path, sizeof(path));

    if (oflag & O_CREAT) {
        if (mkfifo(path, 0666) < 0 && errno != EEXIST) {
            return MQD_INVALID;
        }
    }
    return open(path, O_RDWR);
}

static inline int mq_close(mqd_t mqdes)
{
    return close(mqdes);
}

static inline int mq_unlink(const char *name)
{
    char path[256];
    _mq_to_path(name, path, sizeof(path));
    return unlink(path);
}

/*
 * mq_send: write a 4-byte LE length prefix followed by the payload.
 * This preserves message boundaries over the stream-oriented FIFO.
 */
static inline int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
                           unsigned int msg_prio)
{
    (void)msg_prio;
    uint32_t hdr = (uint32_t)msg_len;
    unsigned char buf[4];
    buf[0] = (unsigned char)(hdr & 0xff);
    buf[1] = (unsigned char)((hdr >> 8) & 0xff);
    buf[2] = (unsigned char)((hdr >> 16) & 0xff);
    buf[3] = (unsigned char)((hdr >> 24) & 0xff);

    if (write(mqdes, buf, 4) != 4) {
        return -1;
    }
    ssize_t n = write(mqdes, msg_ptr, msg_len);
    return (n == (ssize_t)msg_len) ? 0 : -1;
}

/* Helper: read exactly n bytes, retrying on partial reads. */
static inline int _mq_read_all(int fd, void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t r = read(fd, (char *)buf + done, n - done);
        if (r <= 0) {
            return -1;
        }
        done += (size_t)r;
    }
    return 0;
}

/*
 * mq_receive: read the 4-byte LE length prefix, then read that many bytes
 * into msg_ptr (clamped to msg_len).
 */
static inline ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                                  unsigned int *msg_prio)
{
    unsigned char hdr[4];
    if (_mq_read_all(mqdes, hdr, 4) < 0) {
        return -1;
    }
    uint32_t payload_len = (uint32_t)hdr[0]
                         | ((uint32_t)hdr[1] << 8)
                         | ((uint32_t)hdr[2] << 16)
                         | ((uint32_t)hdr[3] << 24);

    size_t to_read = (payload_len < msg_len) ? payload_len : msg_len;
    if (_mq_read_all(mqdes, msg_ptr, to_read) < 0) {
        return -1;
    }
    if (msg_prio) {
        *msg_prio = 0;
    }
    return (ssize_t)to_read;
}

static inline int mq_getattr(mqd_t mqdes, struct mq_attr *attr)
{
    (void)mqdes;
    if (!attr) {
        return -1;
    }
    attr->mq_flags   = 0;
    attr->mq_maxmsg  = 16;
    attr->mq_msgsize = 4096;
    attr->mq_curmsgs = 0;
    return 0;
}

#endif /* MACOS_MQUEUE_H */
