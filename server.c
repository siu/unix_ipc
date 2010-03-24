#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#define ITER_DELAY_NSECS 300000

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

    //fcntl(incoming, F_SETFL, O_NONBLOCK);

    server_fd = client_fd = incoming;
    return 0;

}
int server_end()
{
    if (client_fd != -1) 
        close(client_fd);
    if (server_socket != -1) {
        shutdown(server_socket, 0);
        close(server_socket);
    }

    server_socket = server_fd = client_fd = -1;

    return 0;
}
#endif

/* This server simply echoes everything */
void server_run()
{
    char line[256];
    int len;

    int pcount = 0;
    int total_pcount = 0;
    int turn = 1;

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

        if (line[0] == 'D') 
        {
            printf(".");
            nanosleep(&wait_time, NULL);
            write_str(client_fd, "%s\n", line);

            pcount++;
            total_pcount++;
            continue;
        }
        if (line[0] == 'T') 
        {

            // End of turn
            printf("<T\n");
            write_str(client_fd, "T\n");

            printf("Stats: turn %d, particles %d\n", turn, pcount);
            pcount = 0; turn++;
            continue;
        }
        if (line[0] == 'E') 
        {
            // End of computation
            // Send E and expect a client disconnect
            printf("<E\n");
            write_str(client_fd, "E\n");

            printf("Stats: completed turns %d, total particles %d\n", turn-1, total_pcount);
            break;
        }
        fprintf(stderr, "Unrecognized line format, line: \"%s\"\n", line);
    }
}

void sigalrm_handler (int sig)
{
    fprintf(stderr, "Timeout\n");
    server_end();
    exit(1);
}

void setup_signals()
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigalrm_handler;

    if (sigaction(SIGALRM, &act, NULL) < 0) 
    {
        fprintf(stderr, "Error installing SIGALRM handler\n");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    setup_signals();

    printf("Initializing server and waiting connection...\n");
    if (server_init() != -1) 
    {
        printf("Listening for data...\n");
        server_run();
    }
    printf("Closing server...\n");
    server_end();

    return 0;
}
