#include "wrapper.h"

int write_str(int fd, const char *fmt, ...) __attribute__((format (printf, 2, 3)));

/*
 * xwrite() is the same a write(), but it automatically restarts write()
 * operations with a recoverable error (EAGAIN and EINTR). xwrite() DOES NOT
 * GUARANTEE that "len" bytes is written even if the operation is successful.
 */
ssize_t xwrite(int fd, const void *buf, size_t len)
{
    ssize_t nr;
    while (1) {
        nr = write(fd, buf, len);
        if ((nr < 0) && (errno == EAGAIN || errno == EINTR))
            continue;
        return nr;
    }
}

/*
 * Write a packetized stream, where each line is preceded by
 * its length (including the header) as a 4-byte hex number.
 * A length of 'zero' means end of stream (and a length of 1-3
 * would be an error).
 *
 * This is all pretty stupid, but we use this packetized line
 * format to make a streaming format possible without ever
 * over-running the read buffers. That way we'll never read
 * into what might be the pack data (which should go to another
 * process entirely).
 *
 * The writing side could use stdio, but since the reading
 * side can't, we stay with pure read/write interfaces.
 */
ssize_t safe_write(int fd, const void *buf, ssize_t n)
{
    ssize_t nn = n;
    while (n) {
        int ret = xwrite(fd, buf, n);
        if (ret > 0) {
            buf = (char *) buf + ret;
            n -= ret;
            continue;
        }
        if (!ret)
        {
            fprintf(stderr, "Write error, disk full?\n");
            return -1;
        }
        fprintf(stderr, "Write error\n");
        return -1;
    }
    return nn;
}


static char out_buffer[1024];
int write_str(int fd, const char *fmt, ...) 
{
    va_list args;
    unsigned n;

    va_start(args, fmt);
    n = vsnprintf(out_buffer, sizeof(out_buffer), fmt, args);
    va_end(args);
    if (n >= sizeof(out_buffer))
    {
        fprintf(stderr, "Cannot write to out_buffer, line too long:\n\t%s\n", 
                strerror(errno));
        return -1;
    }
        
    safe_write(fd, out_buffer, n);
    return 0;
}

static char in_buffer[1024];
static int  in_buf_len = 0;
ssize_t read_line(int fd, char *line, size_t linelen)
{
    char *in_bufp = in_buffer+in_buf_len;
    int in_buf_read = 0;
    int len;

    while (1)
    {
        for (; in_buf_read<in_buf_len; in_buf_read++) {
            if (in_buffer[in_buf_read] == '\n')
            {
                break;
            }
        }
        if (in_buf_read < in_buf_len || in_buf_len == 1024) {
            break;
        }

        len = xread(fd, in_bufp, 1024 - in_buf_len);
        if (len <= 0) 
        {
            return -1;
        }

        in_buf_len += len;
        in_bufp += len;

    }

    in_buf_read++;
    if (in_buf_read <= linelen)
    {
        strncpy(line, in_buffer, in_buf_read-1);
        memmove(in_buffer, in_buffer+in_buf_read, 1024 - in_buf_read);
        in_buf_len -= in_buf_read;
        return in_buf_read -1;
    }

    fprintf(stderr, "Error reading line, line too long\n");
    return -1;
}

/*
 * xread() is the same a read(), but it automatically restarts read()
 * operations with a recoverable error (EAGAIN and EINTR). xread()
 * DOES NOT GUARANTEE that "len" bytes is read even if the data is available.
 */
ssize_t xread(int fd, void *buf, size_t len)
{
    ssize_t nr;
    while (1) {
        nr = read(fd, buf, len);
        if ((nr < 0) && (errno == EAGAIN || errno == EINTR))
            continue;
        return nr;
    }
}
