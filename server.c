#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "wrapper.h"

#define ITER_DELAY_SECS 0
//#define ITER_DELAY_NSECS 300000
#define ITER_DELAY_NSECS 0

bool verbose = false;

#ifndef NET
// Pipes
#define SERVER_FILENAME "server.pipe"
#define CLIENT_FILENAME "client.pipe"

#else 
// Network
#define SERVER_HOST "localhost"
#define SERVER_PORT 1355

static int server_socket = -1;

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

int server_end()
{
    int ret;

    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);

    server_fd = client_fd = -1;

    if((ret = unlink(SERVER_FILENAME)) < 0)
        fprintf(stderr, "An error ocurred when atempting to remove \"%s\"\n", SERVER_FILENAME);
    if((ret = unlink(CLIENT_FILENAME)) < 0)
        fprintf(stderr, "An error ocurred when atempting to remove \"%s\"\n", CLIENT_FILENAME);

    return 0;
}

#else
int server_init()
{
    struct sockaddr_in ai;

    server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0)
    {
        fprintf(stderr, "An error ocurred when creating the server socket:\n\t%s\n", strerror(errno));
        return -1;
    }
    
    memset(&ai, 0, sizeof(ai));
    ai.sin_family = PF_INET;
    ai.sin_addr.s_addr = htonl(INADDR_ANY);
    ai.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&ai, sizeof(ai)) < 0)
    {
        close(server_socket);
        server_socket = -1;
        fprintf(stderr, "Can't bind to address:\n"
                        "\t%s\n", strerror(errno));
        return -1;
    }
    if (listen(server_socket, 5) < 0)
    {
        close(server_socket);
        server_socket = -1;
        fprintf(stderr, "Can't listen:\n"
                        "\t%s\n", strerror(errno));
        return -1;
    }

    int incoming = -1;
    struct sockaddr_storage ss;
    unsigned int sslen = sizeof(ss);
    do {
        incoming = accept(server_socket, (struct sockaddr *)&ss, &sslen);

        if(incoming < 0) {
            switch(errno) {
                case EAGAIN:
                case EINTR:
                case ECONNABORTED:
                    continue;
                default:
                    break;
                }
        }
    } while (incoming < 0);

    if (incoming < 0) {
        close(server_socket);
        server_socket = -1;
        fprintf(stderr, "Can't listen:\n"
                        "\t%s\n", strerror(errno));
        return -1;
    }

    server_fd = client_fd = incoming;
    return 0;

}
int server_end()
{
    if (client_fd != -1) 
        close(client_fd);
    if (server_socket != -1) {
        close(server_socket);
        shutdown(server_socket, 0);
    }

    server_socket = server_fd = client_fd = -1;

    return 0;
}
#endif

#ifdef FILEDUMP
FILE * indump = NULL;
FILE * outdump = NULL;
void open_dump_files()
{
    indump = fopen("server_in.log", "w+");
    outdump = fopen("server_out.log", "w+");
}
void close_dump_files()
{
    if (indump)
        fclose(indump);
    if (outdump)
        fclose(outdump);
    indump = outdump = NULL;
}

#endif

/* This server simply echoes everything */
void server_run()
{
    char line[256];
    int len;

    unsigned int pcount = 0;
    unsigned int total_pcount = 0;
    unsigned int turn = 1;

    struct timespec wait_time;
    wait_time.tv_sec = ITER_DELAY_SECS;
    wait_time.tv_nsec = ITER_DELAY_NSECS;

    /*
     *  We'll ignore SIGTERM from now on, we have a
     *  good client.
     */
    //signal(SIGTERM, SIG_IGN);

    while (1)
    {
        len = read_line(server_fd, line, sizeof(line));
        
        if (len == 0)
            continue;
        if (len < 0) 
        {
            fprintf(stderr, "Connection closed by client\n");
            break;
        }

#ifdef DEBUG
        printf("read: \"%s\"\n", line);
#endif
#ifdef FILEDUMP
        fprintf(indump, "%s\n", line);
#endif

        if (line[0] == 'D') 
        {
            if (verbose) printf("<");

            nanosleep(&wait_time, NULL);
            write_str(client_fd, "%s\n", line);
#ifdef FILEDUMP
            fprintf(outdump, "%s\n", line);
#endif

            if (verbose) printf(".");

            pcount++;
            continue;
        }
        if (line[0] == 'T') 
        {

            // End of turn
            if (verbose) printf("<T\n");
            write_str(client_fd, "T\n");
#ifdef FILEDUMP
            fprintf(outdump, "T\n");
#endif

            printf("Stats: turn %u, particles %u\n", turn, pcount);
            total_pcount += pcount;
            pcount = 0; 
            turn++;
            continue;
        }
        if (line[0] == 'E') 
        {
            // End of computation
            // Send E and expect a client disconnect
            if (verbose) printf("<E\n");
            write_str(client_fd, "E\n");
#ifdef FILEDUMP
            fprintf(outdump, "E\n");
#endif

            printf("Stats: completed turns %u, total particles %u\n", turn-1, total_pcount);
            break;
        }
        fprintf(stderr, "Unrecognized line format, line: \"%s\"\n", line);
    }
}

void fatal_signal (int sig)
{
    if (sig == SIGALRM)
        fprintf(stderr, "Timeout\n");
    printf("Closing server...\n");
    server_end();
#ifdef FILEDUMP
    close_dump_files();
#endif
    exit(1);
}

int main(int argc, char *argv[])
{
    signal(SIGALRM, fatal_signal);
    signal(SIGINT, fatal_signal);
    signal(SIGTERM, fatal_signal);

    printf("Initializing server and waiting connection...\n");
    if (server_init() != -1) 
    {
#ifdef FILEDUMP
        open_dump_files();
#endif
        printf("Listening for data...\n");
        server_run();
    }
    printf("Closing server...\n");
    server_end();
#ifdef FILEDUMP
    close_dump_files();
#endif

    return 0;
}
