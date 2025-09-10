#include "server.h"

struct Client* handle_new_connection(int, ssize_t, char[], char*);
int create_directories(const char[]);
void timeLog(char, int, bool);
void err(char*, int);
int create_directory_path(const char*);
int sanInt(char*);
char* sanFilename(char*);
int get_absolute_path(const char*, char*, size_t);
void handle_shutdown(int);
int lock_file(int, int);

int main(int argc, char** argv)
{ 
    //Sanitize arguments
    if(argc != 3)
	err("Usage: ./myserver <port> <dopppc>", 1);

    //Sanitize port
    int server_port = sanInt(argv[1]); 
    if(server_port < 1 || server_port > 65535)
	err("Port must be 1-65535", 1);

    //Sanitize drop rate
    int droppc = sanInt(argv[2]);
    if(droppc < 0 || droppc > 100)
	err("Drop rate must be 0-100", 1);

    /*
    //Sanitize input file
    char* directory = sanFilename(argv[3]);
    if(directory == NULL)
        err("Error in directory naming", 1);

    if(create_directory_path(directory) != 0)
 	    err("Error creating directory", 1);
    
*/
   char directory[1024];
   if (getcwd(directory, sizeof(directory)) != NULL) 
   {
        //printf("Current working directory: %s\n", cwd);
   } 
   else 
   {
        perror("getcwd() error");
        return 1;
   }


   signal(SIGTERM, handle_shutdown);
   signal(SIGINT, handle_shutdown);

    //End input sanitization
    //Start sockets
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in server_addr, client_addr;
    unsigned int clientLength = sizeof(client_addr);

    if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	err("Issue Connecting Socket", 1);

    bzero(&server_addr, sizeof(server_addr));
    bzero(&client_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
	close(sockfd);
	err("Error binding to port", 1);
    }

    struct Client *clients[MAX_CLIENTS];

    bzero(&client_addr, sizeof(server_addr));
    memset(buffer, 0, MAXLINE);

    srand(time(NULL));
    //int out_file_len = 0;
    while(1)
    {	
        bzero(&client_addr, sizeof(server_addr));
        memset(buffer, 0, MAXLINE);

        //Recieve client connection
        ssize_t recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &clientLength);

	/*
	int r = (rand() % 100) + 1;
	if(r <= droppc)
	{
	    printf("Dropped Packet\n");
	    continue;
	}
        */
	
	if((int)recv_len < (int)(sizeof(char) + sizeof(int)))
	{
	    usleep(100000);
	    continue;
	}
        

        char code;
        memcpy(&code, buffer, sizeof(char));
	
	if(code == 'S')
	{
	    for(int i = 0; i < MAX_CLIENTS; i++)
	    {
		if(clients[i] == NULL)
		{
	            clients[i] = handle_new_connection(sockfd, recv_len, buffer, directory);
	            clients[i]->id = i;  
		    clients[i]->client_addr = client_addr;
		    clients[i]->clientLength = clientLength;

		    		    
                    code = 'A';
    	            char ackpkt[sizeof(char) + sizeof(int)];
    
        	    memcpy(ackpkt, &code, sizeof(char));
        	    memcpy(ackpkt + sizeof(char), &clients[i]->id, sizeof(int));

       	 	    if (sendto(sockfd, ackpkt, sizeof(char) + sizeof(int), 0, (struct sockaddr*)&clients[i]->client_addr, clients[i]->clientLength) < 0)
         	    {
            		close(sockfd);
	    		err("Error sending ACK", 1);  //shouldnt exit;
        	    }
		   
		    if(lock_file(clients[i]->fd, F_WRLCK) == -1)
		    {
			printf("writing to file\n");
			sleep(5);

			code = 'W';
			memcpy(ackpkt, &code, sizeof(char));
			if (sendto(sockfd, ackpkt, sizeof(char) + sizeof(int), 0, (struct sockaddr*)&clients[i]->client_addr, clients[i]->clientLength) < 0)
			{
			    close(sockfd);
			    err("Error sending wait\n", 1);
			}
		    }

		    break;
		}
	    }
	    continue;
	}

	int id;
        memcpy(&id, buffer + sizeof(char), sizeof(int));

	if(id < 0 || id > MAX_CLIENTS)
	    continue;
	else if(clients[id] == NULL)
	    continue;

        memcpy(&clients[id]->client_seq, buffer + sizeof(char) + sizeof(int), sizeof(int));

	if(clients[id]->client_seq < 0)
	    continue;
	
	//rand
	int r = (rand() % 100) + 1;
	if(r <= droppc)
	{
	    printf("Server dropped a packet\n");
	    continue;
	}

	if(code == 'D')
	{
	    char file_contents[recv_len - sizeof(char) - sizeof(int) - sizeof(int)];

	    memcpy(file_contents, buffer + sizeof(char) + sizeof(int) + sizeof(int), sizeof(file_contents));
	    file_contents[recv_len - HEADER] = '\0';

	    timeLog(code, clients[id]->client_seq, false);

	   if(clients[id]->client_seq == clients[id]->server_seq)
	   {
	        write(clients[id]->fd, file_contents, recv_len - sizeof(char) - sizeof(int) - sizeof(int));
	        //printf("wrote: %d %s\n", clients[id]->server_seq, file_contents);
		clients[id]->server_seq++;
		clients[id]->client_sliding_window++;
	   }
	   else
	   {

		char chk_pkt[sizeof(char) + sizeof(int)];

  
		if(clients[id]->client_seq == clients[id]->server_seq - 1)
		    code = 'A';
		else
                    code = 'N';

                memcpy(chk_pkt, &code, sizeof(char));
                memcpy(chk_pkt + sizeof(char), &clients[id]->server_seq, sizeof(int));

		//printf("sent server seq: %d\n", clients[id]->server_seq);

		timeLog(code, clients[id]->server_seq, false); 
	
                if (sendto(clients[id]->sockfd, chk_pkt, sizeof(chk_pkt), 0, (struct sockaddr*)&clients[id]->client_addr, clients[id]->clientLength) < 0)
                {
                    err("Error sending ACK", 1);  //shouldnt exit;
                }
		
		clients[id]->client_sliding_window = 0;
	   }
	   if(clients[id]->client_sliding_window >= clients[id]->client_window)
	   {
		char ack_pkt[sizeof(char) + sizeof(int)];

		code = 'A'; 
		memcpy(ack_pkt, &code, sizeof(char));
                memcpy(ack_pkt + sizeof(char), &clients[id]->server_seq, sizeof(int));

		timeLog(code, clients[id]->client_seq, false); 

	 	if (sendto(clients[id]->sockfd, ack_pkt, sizeof(char) + sizeof(int), 0, (struct sockaddr*)&clients[id]->client_addr, clients[id]->clientLength) < 0)
        	{
            	    err("Error sending ACK", 1);  //shouldnt exit;
        	}
		clients[id]->client_sliding_window = 0;
	   }
	}
	else if(code == 'E' || code == 'F')
	{
	    if(clients[id]->client_seq == clients[id]->server_seq)
	    {
		char ack_pkt[sizeof(char) + sizeof(int)];                                                   
                code = 'F';
                memcpy(ack_pkt, &code, sizeof(char));
                memcpy(ack_pkt + sizeof(char), &clients[id]->client_seq, sizeof(int));

		timeLog(code, clients[id]->client_seq, false); 

                if (sendto(clients[id]->sockfd, ack_pkt, sizeof(char) + sizeof(int), 0, (struct sockaddr*)&clients[id]->client_addr, clients[id]->clientLength) < 0)
                {
                    err("Error sending ACK", 1);
		}

		if(lock_file(clients[id]->fd, F_UNLCK) == -1) 
		{
		    err("Error unlocking file\n", 1);
		}

		if(clients[id] != NULL)
		{
		    free(clients[id]);
		    clients[id] = NULL;
		    continue;
		}

	    }
	    else
	    {
		char nak_pkt[sizeof(char) + sizeof(int)];    

		code = 'N';              
		memcpy(nak_pkt, &code, sizeof(char));    
		memcpy(nak_pkt + sizeof(char), &clients[id]->server_seq + 1, sizeof(int)); 
		
		timeLog(code, clients[id]->client_seq, false); 

		if (sendto(clients[id]->sockfd, nak_pkt, sizeof(char) + sizeof(int), 0, (struct sockaddr*)&clients[id]->client_addr, clients[id]->clientLength) < 0)       
		{             
		     err("Error sending ACK", 1);
		}
	    }
	}
	else
	{
	    char nak_pkt[sizeof(char) + sizeof(int)];

	    code = 'R';
	    int error_code = -1;

	    memcpy(nak_pkt, &code, sizeof(char));
	    memcpy(nak_pkt + sizeof(char), &error_code, sizeof(int));

	    timeLog(code, error_code, false);

	    if(sendto(clients[id]->sockfd, nak_pkt, sizeof(char) + sizeof(int), 0, (struct sockaddr*)&clients[id]->client_addr, clients[id]->clientLength) < 0)
	    {
		err("Error sending ACK", 1);
	    } 
	}
	

    }
}

struct Client* handle_new_connection(int sockfd, ssize_t recv_len, char buffer[MAXLINE], char* path)
{
    if (recv_len - (sizeof(char) + 2*sizeof(int)) >= 1024) 
        err("Path too long in received data", 1);

    struct Client *client = malloc(sizeof(struct Client));
    if (client == NULL) 
        return NULL;

    client->sockfd = sockfd;

    char p[1024];
    size_t path_len = recv_len - sizeof(char) - 2*sizeof(int);
    
    memcpy(&client->client_mss, buffer + sizeof(char), sizeof(int));
    memcpy(&client->client_window, buffer + sizeof(char) + sizeof(int), sizeof(int));
    memcpy(p, buffer + sizeof(char) + sizeof(int) + sizeof(int), path_len);
    p[path_len] = '\0';

    //printf("File: %s\n", p);

    if (snprintf(client->out_file_path, MAXLINE, "%s/%s", path, p) >= MAXLINE) {
        err("Combined path too long", 1);
        free(client);
        return NULL;
    }

    //printf("New client - mss: %d, window: %d, out_file: %s\n", client->client_mss, client->client_window, client->out_file_path);

    timeLog('S', 0, false);

    client->server_seq = 0;
    client->client_seq = 0;
 
    if(create_directories(client->out_file_path) < 0)     
    {
	err("Cannot create directories", 1);
    } 

    //printf("%s\n", client->out_file_path);
    client->fd = open(client->out_file_path, O_CREAT | O_WRONLY | O_APPEND, 0777);
    if(client->fd < 0) 
    {
	err("Failed to open file", 1);
    }

    return client;
}

int create_directories(const char path[])
{
    char tmp[256];
    char dir_copy[256];
    char *p = NULL;
    size_t len;

    strncpy(dir_copy, path, sizeof(dir_copy) - 1);
    dir_copy[sizeof(dir_copy) - 1] = '\0';

    char *last_slash = strrchr(dir_copy, '/');
    if (last_slash != NULL)
    {
        *last_slash = '\0'; 
    }
    else
    {
        return 0;
    }

    strncpy(tmp, dir_copy, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    len = strlen(tmp);

    if (len > 0 && tmp[len - 1] == '/') 
    {
        tmp[len - 1] = '\0';
        len--;
    }

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/') 
	{
            *p = '\0';
            if (mkdir(tmp, 0755) != 0) 
	    {
                if (errno != EEXIST) 
		{
                    return -1;
                }
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0)
    {
        if (errno != EEXIST) 
	{
            return -1;
        }
    }

    return 0;
}

void timeLog(char code, int server_seq, bool drop)
{
    struct timeval tv;
    struct tm *timeinfo;
    char buffer[80];

    gettimeofday(&tv, NULL);
    timeinfo = gmtime(&tv.tv_sec);
    
    //time creation
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", timeinfo);
	
    char return_code[5] = {0};
    switch(code)
    {
	case 'A' : 
            strcpy(return_code, "ACK");
	    break;

	case 'D' :
	    strcpy(return_code, "DATA");
	    break;

	case 'E' :
	    strcpy(return_code, "EOF");
	    break;

	case 'F' :
	    strcpy(return_code, "FIN");
	    break;

	case 'S' :
	    strcpy(return_code, "SYN");
	    break;

	default:
	    return;

    }

    char drop_pkt[6];
    if(drop)
	strcpy(drop_pkt, "DROP ");
    else
	drop_pkt[0] = '\0';

    printf("%s.%03ldZ, %s%s, %d\n", buffer, tv.tv_usec / 1000, drop_pkt, return_code, server_seq);

}

void err(char* msg, int code)
{
    fprintf(stderr, "%s\n", msg);
    exit(code);
}

int create_directory_path(const char *path) {
    char *tmp = strdup(path);
    char *p = tmp;
    int status = 0;
    
    while (*p == '/') 
	p++;
    
    while (*p != '\0') 
    {
        if (*p == '/') 
	{
            *p = '\0'; 
            
            if (mkdir(tmp, 0755) != 0)
	    {
                if (errno != EEXIST) 
		{
                    status = -1;
                    break;
                }
            }
            
            *p = '/'; 
        }
        p++;
    }
    
    if (status == 0 && strlen(tmp) > 0) 
    {
        if (mkdir(tmp, 0755) != 0) 
	{
            if (errno != EEXIST)
	    {
                status = -1;
            }
        }
    }
    
    free(tmp);
    return status;
}

int sanInt(char* char_port)
{
    regex_t regex;

    const char* pattern = "^[0-9]+$";

    if(regcomp(&regex, pattern, REG_EXTENDED) != 0)
    {
        regfree(&regex);
        return -1;
    }

    int exec = regexec(&regex, char_port, 0, NULL, 0);

    regfree(&regex);

    if(exec != 0)
    {
        return -1;
    }

    return strtol(char_port, NULL, 10);
}

char* sanFilename(char* url)
{
    regex_t regex;
    const char* pattern = "^[a-zA-Z0-9$-_.+!*'(),]+$";

    if(regcomp(&regex, pattern, REG_EXTENDED) != 0)
    {
        return NULL;
    }

    int exec = regexec(&regex, url, 0, NULL, 0);

    regfree(&regex);

    if(exec == 0)
        return url;
    else
        return NULL;
}

int get_absolute_path(const char *relative_path, char *absolute_path, size_t buffer_size) {
    char *resolved_path = realpath(relative_path, absolute_path);

    if (resolved_path == NULL)
    {
        char current_dir[MAXLINE];
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) 
	{
            return -1;
        }

        if (relative_path[0] == '/')
            strncpy(absolute_path, relative_path, buffer_size - 1);
	else
            snprintf(absolute_path, buffer_size, "%s/%s", current_dir, relative_path);

        absolute_path[buffer_size - 1] = '\0';
        return 0;
    }

    return 0;
}
void handle_shutdown(int signum) 
{
    (void) signum;
    exit(0);
}

int lock_file(int fd, int lock_type)
{
    struct flock fl;

    fl.l_type = lock_type;   
    fl.l_whence = SEEK_SET;   
    fl.l_start = 0;         
    fl.l_len = 0;            
    fl.l_pid = getpid();      

    return fcntl(fd, F_SETLKW, &fl);
}
