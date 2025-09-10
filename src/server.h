#include <stdio.h>

#include <unistd.h>
#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <libgen.h>
#include <errno.h>
#include <signal.h>

#define MAXLINE 10000
#define HEADER 9
#define MAX_TIMEOUT 4
#define MAX_CLIENTS 10

struct Client
{
    int id;
    
    int fd;
    int sockfd;
    
    struct sockaddr_in client_addr;
    int clientLength;
    
    char out_file_path[MAXLINE];
       
    int client_mss;
    int client_window;
    int client_sliding_window;

    int server_seq;
    int client_seq;

    int retries;

};

