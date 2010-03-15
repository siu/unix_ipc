#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include "wrapper.h"

#ifndef NET
// Pipes
#define SERVER_FILENAME "server.pipe"
#define CLIENT_FILENAME "client.pipe"

#else 
// Network
#define SERVER_HOST "localhost"
#define SERVER_PORT 1355

#endif

static int client_fd = -1;
static int server_fd = -1;

#ifndef NET
int server_init()
{
    int ret;

    ret = mkfifo(SERVER_FILENAME, S_IRUSR | S_IWUSR);
    if (ret < 0)
    {

        fprintf(stderr, "An error ocurred when creating the fifo file \"%s\":\n\t%s\n",
                SERVER_FILENAME, strerror(errno));
        return -1;
    }

    ret = mkfifo(CLIENT_FILENAME, S_IRUSR | S_IWUSR);
    if (ret < 0)
    {

        fprintf(stderr, "An error ocurred when creating the fifo file \"%s\":\n\t%s\n",
                CLIENT_FILENAME, strerror(errno));
        return -1;
    }

    server_fd = open(SERVER_FILENAME, O_RDONLY);
    if (server_fd < 0) {
        fprintf(stderr, "An error ocurred when opening file \"%s\":\n\t%s\n", 
                SERVER_FILENAME, strerror(errno));
        return -1;
    }

    client_fd = open(CLIENT_FILENAME, O_WRONLY);
    if (client_fd < 0) {
        fprintf(stderr, "An error ocurred when opening file \"%s\":\n\t%s\n", 
                CLIENT_FILENAME, strerror(errno));
        return -1;
    }

    return 0;
}

#else
int server_init()
{
    return 0;
}
#endif

/* This server simply echoes everything */
void server_run()
{
    char line[1024];
    int len;

    while (1)
    {
        len = read_line(server_fd, line, sizeof(line));
        
        if (len <= 0) 
        {
            fprintf(stderr, "Connection closed by client\n");
            break;
        }

#ifdef DEBUG
        printf("read: \"%s\"\n", line);
#endif

        if(line[0] == 'D') {
            printf(".");
            write_str(client_fd, "%s\n", line);
            continue;
        }
        if(line[0] == 'T') {

            // End of turn
            printf("<T\n");
            write_str(client_fd, "T\n");
            continue;
        }
        if(line[0] == 'E') {
            // End of computation
            // Send E and expect a client disconnect
            printf("<E\n");
            write_str(client_fd, "E\n");
            break;
        }
    }
}

int server_end()
{
    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);
#ifndef NET
    int ret;
    if((ret = unlink(SERVER_FILENAME)) < 0)
    {
        fprintf(stderr, "An error ocurred when atempting to remove \"%s\"\n", SERVER_FILENAME);
    }
    if((ret = unlink(CLIENT_FILENAME)) < 0)
    {
        fprintf(stderr, "An error ocurred when atempting to remove \"%s\"\n", CLIENT_FILENAME);
    }
#endif
    return 0;
}

int main(int argc, char *argv[])
{
    printf("Initializing server...\n");
    if (server_init() != -1) 
    {
        printf("Listening for data...\n");
        /*
         *  We'll ignore SIGTERM from now on, we have a
         *  good client.
         */
        signal(SIGTERM, SIG_IGN);

        server_run();
    }
    printf("Closing server...\n");
    server_end();

    return 0;
}
