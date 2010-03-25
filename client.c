#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include "wrapper.h"

#define TURNS 500
#define NPARTS 2000
//#define TURN_DELAY_SECS 1
#define TURN_DELAY_SECS 0
#define TURN_DELAY_NSECS 0

bool verbose = false;

#ifndef NET
// Pipes
#define SERVER_FILENAME "server.pipe"
#define CLIENT_FILENAME "client.pipe"

#else 
// Network
#define SERVER_HOST "localhost"
#define SERVER_PORT 1355

#endif

static int server_fd = -1;
static int client_fd = -1;

#ifdef FILEDUMP
FILE * indump = NULL;
FILE * outdump = NULL;
void open_dump_files()
{
    indump = fopen("client_in.log", "w+");
    outdump = fopen("client_out.log", "w+");
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

/**
 * Generate a particle string
 * @param turn: turn number
 * @param id: particle id
 * @return New particle string
 */
static char *generate_particle(int turn, int id) 
{
    char buf[256];
    char *part;

    snprintf(buf, 1024, "D %d %d %7e %7e %7e %7e %7e %d %d %7e %7e", 
            turn, id, 
            (float)rand()/RAND_MAX, 
            (float)rand()/RAND_MAX, 
            (float)rand()/RAND_MAX, 
            (float)rand()/RAND_MAX, 
            (float)rand()/RAND_MAX,
            1, 1,
            (float)rand()/RAND_MAX, 
            (float)rand()/RAND_MAX);

    part = malloc(sizeof(char) * strlen(buf) + 1);
    if (part) 
        memcpy(part, buf, strlen(buf)+1);
    return part;
    
}

static unsigned int in_particles = 0;
/**
 * Process incoming particles if any
 * returns true if end of turn is received
 */
bool process_incomming_particles()
{
    char buf[1024];
    int len;

    while (1)
    {
        len = read_line(client_fd, buf, sizeof(buf));
        
        if (len == 0)
            break;
        if (len < 0) 
        {
            fprintf(stderr, "Connection closed by server\n");
            break;
        }

#ifdef DEBUG
        printf("read: \"%s\"\n", buf);
#endif
#ifdef FILEDUMP
        fprintf(indump, "%s\n", buf);
#endif

        if (buf[0] == 'D')
        {
            if (verbose) printf("<");

            in_particles++;
            continue;
        }
        if (buf[0] == 'T') 
        {

            // End of turn
            if (verbose) printf("<T\n");
            return true;
        }
    }
    return false;
}
void wait_server_end()
{
    char buf[1024];
    int len;

    while (1)
    {
        len = read_line(client_fd, buf, sizeof(buf));
        
        if (len == 0)
            continue;
        if (len < 0) 
        {
            fprintf(stderr, "Connection closed by server\n");
            break;
        }

#ifdef DEBUG
        printf("read: \"%s\"\n", buf);
#endif
#ifdef FILEDUMP
        fprintf(indump, "%s\n", buf);
#endif

        if (buf[0] == 'E') 
        {
            // End of computation
            if (verbose) printf("<E\n");
            break;
        }
    }
}

#ifndef NET
/**
 * server_connects to server through a pipe
 */
int server_connect(char *server_filename, char *client_filename)
{
    server_fd = open(server_filename, O_WRONLY);
    if (server_fd < 0) {
        fprintf(stderr, "An error ocurred when opening file \"%s\":\n\t%s\n", 
                server_filename, strerror(errno));
        return -1;
    }

    client_fd = open(client_filename, O_RDONLY | O_NONBLOCK);
    if (client_fd < 0) {
        fprintf(stderr, "An error ocurred when opening file \"%s\":\n\t%s\n", 
                client_filename, strerror(errno));
        return -1;
    }

    return 0;
}

void server_disconnect()
{
    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);
    server_fd = client_fd = -1;
}
#else

/**
 * Connects to server through tcp/ip
 */
int server_connect(char *host, int portnum)
{
    int sockfd = -1;
    struct sockaddr_in sin;

    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        fprintf(stderr, "Unable to create socket (%s)\n", strerror(errno));
        return -1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(portnum);

    if (connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        fprintf(stderr, "Unable to connect to %s %d error=(%s)\n", host, portnum, strerror(errno));
        close(sockfd);
        sockfd = -1;
        return -1;
    }
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    server_fd = client_fd = sockfd;

    return sockfd;
}

void server_disconnect()
{
    if (server_fd != -1) close(server_fd);
    server_fd = client_fd = -1;
}
#endif // Net

int main(int argc, char *argv[])
{
    int ret;
    char *part;
    int turn, pid;
    int t, p;
    bool turn_done;
    struct timespec wait_time;
    wait_time.tv_sec = TURN_DELAY_SECS;
    wait_time.tv_nsec = TURN_DELAY_NSECS;

    printf("Connecting to server...\n");
#ifndef NET
    ret = server_connect(SERVER_FILENAME, CLIENT_FILENAME);
#else
    ret = server_connect(SERVER_HOST, SERVER_PORT);
#endif
#ifdef FILEDUMP
    open_dump_files();
#endif

    if (ret != -1)
        printf("Sending data...\n");
        for (t=0; t<TURNS; t++)
        {
            turn_done = false;
            turn = t+1;
            in_particles = 0;
            printf("== Turn %d ==\n", turn);
            printf(" * Sending %d particles...\n", NPARTS);
            nanosleep(&wait_time, NULL);
            for (p=0; p<NPARTS && !turn_done; p++) 
            {
                pid = p+1;
                part = generate_particle(turn, pid);
                if (!part)
                {
                    fprintf(stderr, "Error generating particle\n");
                    continue;
                }

#ifdef DEBUG
                printf("Generated particle with id (%d,%d): \"%s\"\n", turn, pid, part);
#endif
                // Send particle
                write_str(server_fd, "%s\n", part);
#ifdef FILEDUMP
                fprintf(outdump, "%s\n", part);
#endif
                if (verbose) printf(".");

                free(part);

                // Process incoming data if any
                turn_done = process_incomming_particles();
            }
            // Send end of turn
            if (verbose) printf("T\n");
            write_str(server_fd, "T %d\n", turn);
#ifdef FILEDUMP
            fprintf(outdump, "T %d\n", turn);
#endif

            // Process incoming data if any
            while(!turn_done && !(turn_done = process_incomming_particles()));
            assert(in_particles == NPARTS);
            printf(" * Received %d particles\n", in_particles);
        }
    if (verbose) printf("E\n");
    write_str(server_fd, "E\n");
#ifdef FILEDUMP
    fprintf(outdump, "E\n");
#endif
    wait_server_end();

    printf("Disconnecting...\n");
    server_disconnect();
#ifdef FILEDUMP
    close_dump_files();
#endif

    return 0;
}
