#include "wrapper.h"

#define IN_BUFFER_SIZE 1024
#define IN_BUFFER_SIZE 1024

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

static char out_buffer[IN_BUFFER_SIZE];
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
        
    return safe_write(fd, out_buffer, n);
}

static char in_buffer[IN_BUFFER_SIZE];
static int  in_buf_len = 0; // Size of data stored in buffer

ssize_t read_line(int fd, char *line, size_t linelen)
{
    char *in_bufp = in_buffer+in_buf_len;
    int line_size = 0;
    int len;

    while (1)
    {
        // Move line_size until a newline is found or end of buffer
        for (; line_size<in_buf_len && in_buffer[line_size] != '\n'; line_size++);
        line_size++; // Honour its name and reflect size, not position

        if (line_size <= in_buf_len)
            break;

        if (in_buf_len >= IN_BUFFER_SIZE) 
        {
            fprintf(stderr, "No end line character found in the buffer (buffer is full)\n");
            return -1;
        }

        alarm(100u);
        len = read(fd, in_bufp, IN_BUFFER_SIZE - in_buf_len);
        alarm(0u);

        if (len == 0) 
            return 0;
        if (len < 0) 
        {
            if(errno == EAGAIN || errno == EINTR)
                return 0;
            return -1;
        }

        in_buf_len += len;
        in_bufp += len;
    }

    if (line_size > linelen)
    {
        fprintf(stderr, "Error reading line, line too long\n");
        return -1;
    }

    strncpy(line, in_buffer, line_size);
    assert( line[line_size-1] == '\n' );
    line[line_size-1] = 0;
    in_buf_len -= line_size;
    memmove(in_buffer, in_buffer+line_size, in_buf_len);
    return line_size;

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
