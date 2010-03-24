#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h>

ssize_t read_line(int fd, char *line, size_t linelen);
int write_str(int fd, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
ssize_t xwrite(int fd, const void *buf, size_t len);
ssize_t safe_write(int fd, const void *buf, ssize_t n);
ssize_t xread(int fd, void *buf, size_t len);
