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
#include <sys/wait.h>
#include <signal.h>
 
#include <sys/types.h>   

#define MAXLINE 10000
#define HEADER 9
#define MAX_RETRIES 4

struct Packet
{
    char pkt[MAXLINE];
};
