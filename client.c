#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
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

static int server_fd = -1;
static int client_fd = -1;

/**
 * Generate a particle string
 * @param turn: turn number
 * @param id: particle id
 * @return New particle string
 */
static char *generate_particle(int turn, int id) 
{
    char buf[1024];
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
void wait_server_particles()
{
    char buf[1024];
    int len;

    while (1)
    {
        len = read_line(client_fd, buf, sizeof(buf));
        
        if (len <= 0) 
        {
            fprintf(stderr, "Connection closed by server\n");
            break;
        }

#ifdef DEBUG
        printf("read: \"%s\"\n", buf);
#endif

        if (buf[0] == 'D')
        {
            printf("<");
            continue;
        }
        if (buf[0] == 'T') 
        {

            printf("<T\n");
            // End of turn
            break;
        }
    }
}
void wait_server_end()
{
    char buf[1024];
    int len;

    while (1)
    {
        len = read_line(client_fd, buf, sizeof(buf));
        
        if (len <= 0) 
        {
            fprintf(stderr, "Connection closed by server\n");
            break;
        }

#ifdef DEBUG
        printf("read: \"%s\"\n", buf);
#endif

        if (buf[0] == 'E') 
        {
            // End of computation
            printf("<E\n");
            break;
        }
    }
}

#ifndef NET
/**
 * Connects to server through a pipe
 */
int connect(char *server_filename, char *client_filename)
{
    server_fd = open(server_filename, O_WRONLY);
    if (server_fd < 0) {
        fprintf(stderr, "An error ocurred when opening file \"%s\":\n\t%s\n", 
                server_filename, strerror(errno));
        return -1;
    }

    client_fd = open(client_filename, O_RDONLY);
    if (client_fd < 0) {
        fprintf(stderr, "An error ocurred when opening file \"%s\":\n\t%s\n", 
                client_filename, strerror(errno));
        return -1;
    }

    return 0;
}
#else
/**
 * Connects to server through tcp/ip connection
 */
int connect(char *hostname, int port)
{
    // TODO
    return 0;
}
#endif // Net

void disconnect()
{
    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);
    server_fd = client_fd = -1;
}

int main(int argc, char *argv[])
{
    int ret;
    char *part;
    int turn, pid;
    int t, p;

    printf("Connecting to server...\n");
#ifndef NET
    ret = connect(SERVER_FILENAME, CLIENT_FILENAME);
#else
    ret = connect(SERVER_HOST, SERVER_PORT);
#endif

    if (ret != -1)
        printf("Sending data...\n");
        for (t=0; t<500; t++)
        {
            turn = t+1;
            for (p=0; p<200; p++) 
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
                printf(".");

                free(part);

            }
            // Send end of turn
            printf("T\n");
            write_str(server_fd, "T %d\n", turn);
            wait_server_particles();
        }
    printf("E\n");
    write_str(server_fd, "E\n");
    wait_server_end();

    printf("Disconnecting...\n");
    disconnect();

    return 0;
}
