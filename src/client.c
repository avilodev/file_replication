#include "client.h"

void timeAckLog(char, int, char*, char*, bool);
void timeLog(char, int, int, int, char*, char*);
void errno(char*, int);
int sanInt(char*);
char* sanFilename(char*);
int start_client_and_server(char*, char*, int, int, char*, char*);
int get_absolute_path(const char*, char*, size_t);
void handle_child_signal(int);
void init_process_management();
void cleanup_processes();
void signal_handler(int);


pid_t *child_pids = NULL; 
int num_children = 0;

int main(int argc, char** argv)
{
    //Make sure there are 6 params
    if(argc != 7)
        errno("Usage: ./myclient server_ip server_port mss winsz in_file_path out_file_path", 1);

    char* server_ip = sanFilename(argv[1]);

    char* serv_addr_port = sanFilename(argv[2]);

    //Sanitize mss
    int mss = sanInt(argv[3]);
    if((long unsigned int)mss < HEADER + sizeof(int) + sizeof(char) || mss > (1500 - HEADER))
    {
        fprintf(stderr, "MSS must range from %d - %d\n", HEADER + 1, 1500 - HEADER);       
        exit(1);
    }

    //Sanitize window size
    int winsz = sanInt(argv[4]);
    if(winsz < 1 || winsz > 50)
        errno("Window size must range from 1-50", 1);

    //Sanitize input file
    char* input_file = sanFilename(argv[5]);
    if(input_file == NULL)
        errno("Error in input file naming", 1);

    struct stat stat_in;
    stat(input_file, &stat_in);
    if(S_ISDIR(stat_in.st_mode))
        errno("Input file is a directory", 1);

    //Sanitize output file
    char* out_file = sanFilename(argv[6]);
    if(out_file == NULL)
        errno("Error in output file naming.", 1);

    struct stat stat_out;
    stat(out_file, &stat_out);
    if(S_ISDIR(stat_out.st_mode))
        errno("Output file is a directory", 1);

/*
    int outfile_size = 0;
    char outfile[MAXLINE] = {0};
    for(unsigned long i = 0; i < strlen(out_file); i++)
    {
        outfile[i] = out_file[i];
        outfile_size++;
    }
    outfile[outfile_size] = '\0';
*/

    int result = start_client_and_server(server_ip, serv_addr_port, mss, winsz, input_file, out_file);
    if (result < 0) {
        errno("Failed to start client", 1);
    }

    signal(SIGINT, signal_handler);

    while (num_children > 0) {
        int status;
        pid_t pid = wait(&status);

        if (pid > 0) {
            for (int i = 0; i < num_children; i++) {
                if (child_pids[i] == pid) {
                    //printf("Process %d completed\n", pid);
                    child_pids[i] = child_pids[num_children - 1];
                    num_children--;
                    break;
                }
            }
        }
    }

    //printf("All processes completed successfully\n");
    return 0;
}


void timeAckLog(char code, int seq_num, char* server_port, char* server_ip, bool drop)
{
    struct timeval tv;
    struct tm *timeinfo;
    char timestamp[80];

    gettimeofday(&tv, NULL);
    timeinfo = gmtime(&tv.tv_sec);

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

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
            strcpy(return_code, "ACK");
            break;

        case 'F' :
        case 'S' :
            strcpy(return_code, "CTRL");
            break;
        default:
            return;

    }

   
    if(drop)
    {
	printf("%s.%03ldZ, %s, %s, %s, DROP %s, %d\n", timestamp, tv.tv_usec / 1000, server_port, server_ip, "9090", return_code, seq_num);
    }
    else
    {
    printf("%s.%03ldZ, %s, %s, %s, %s, %d\n", timestamp, tv.tv_usec / 1000, server_port, server_ip, "9090", return_code, seq_num);
    }
    

}
void timeLog(char code, int seq_num, int base_num, int window_size, char* server_ip, char* server_port)
{
    struct timeval tv;
    struct tm *timeinfo;
    char timestamp[80];

    gettimeofday(&tv, NULL);
    timeinfo = gmtime(&tv.tv_sec);

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

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
            strcpy(return_code, "ACK");
            break;

        case 'F' :
        case 'S' :
            strcpy(return_code, "CTRL");
            break;
        default:
            return;

    }

        printf("%s.%03ldZ, %s, %s, %s, %s, %d, %d, %d, %d\n",timestamp, tv.tv_usec / 1000, server_port, server_ip, "9090", return_code, seq_num, base_num, seq_num + 1, base_num + window_size);



}
void errno(char* msg, int code)
{
    cleanup_processes();
    fprintf(stderr, "%s\n", msg);
    exit(code);
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

    return (strtol(char_port, NULL, 10));
}

char* sanFilename(char* url)
{
    regex_t regex;
    const char* pattern = "^[a-zA-Z0-9$-_.+!*'(),\\/]+$";

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

int start_client_and_server(char* server_ip, char* server_port, int mss, int winsz, char* input_file, char* outfile) 
{
    init_process_management();

    //char buffer[MAXLINE];

    pid_t client_pid = fork();

    if (client_pid < 0) {
	fprintf(stderr, "Failed to fork client process\n");
	exit(1);
    }

    if (client_pid == 0) {

	if (child_pids != NULL) {
	    free(child_pids);
	    child_pids = NULL;
	}

	int sockfd;
	struct sockaddr_in server_addr;
	unsigned int serverLength = sizeof(server_addr);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    errno("Issue Connecting to Socket", 1);
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(server_port));

	if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)        {
	    close(sockfd);
	    errno("IP Address not valid", 1);
	}

	char SYN = 'S';

	char syn_pkt[sizeof(char) + sizeof(int) + sizeof(int) + strlen(outfile)];

	memcpy(syn_pkt, &SYN, sizeof(char));
	memcpy(syn_pkt + sizeof(char), &mss , sizeof(int));
	memcpy(syn_pkt + sizeof(char) + sizeof(int), &winsz, sizeof(int));
	memcpy(syn_pkt + sizeof(char) + sizeof(int) + sizeof(int), outfile, strlen(outfile));

	char recv_ack_buffer[sizeof(char) + sizeof(int)];

	int retries = 0;
	while(retries <= MAX_RETRIES)
	{
	    if(retries == MAX_RETRIES)
	    {
		close(sockfd);
		char err_msg[1024];
		sprintf(err_msg, "%s %s %s %s", "Cannot detect server IP", server_ip, "port", server_port);
		sleep(1);
		errno(err_msg, 3);
	    }

	    fd_set read_fd;
	    struct timeval timeout;
	    timeout.tv_sec = 3;
	    timeout.tv_usec = 0;

	    if(sendto(sockfd, syn_pkt, sizeof(syn_pkt), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	    {
		close(sockfd);
		errno("Error sending Client Hello", 1);
	    }

	    timeAckLog(SYN, 0, server_port, server_ip, false);

	    FD_ZERO(&read_fd);
	    FD_SET(sockfd, &read_fd);

	    int retval = select(sockfd + 1, &read_fd, NULL, NULL, &timeout);        

	    if(retval == -1)
	    {
		close(sockfd);
		errno("Timeout system failed", 1);
	    }
	    else if(retval == 0)
	    {
		fprintf(stderr, "Packet loss detected\n");
		timeAckLog(SYN, 0, server_port, server_ip, true);
		retries++;
		continue;
	    }

	    ssize_t recv_len = recvfrom(sockfd, recv_ack_buffer, sizeof(recv_ack_buffer), 0, (struct sockaddr*)&server_addr, &serverLength);

	    if(recv_len != sizeof(char) + sizeof(int))
	    {
		retries++;
		continue;
	    }

	    char code;
	    memcpy(&code, recv_ack_buffer, sizeof(char));
	    if(code == 'W')
	    {
		fprintf(stderr, "File is busy\n");
		retries = 0;
		continue;
	    }

	    break;
	}

	int bytes_read[winsz];

	char code;
	int id;
	memcpy(&code, recv_ack_buffer, sizeof(char));
	memcpy(&id, recv_ack_buffer + sizeof(char), sizeof(int));

	timeAckLog(code, 0, server_port, server_ip, false);

	int fd_in = open(input_file, O_RDONLY);
	if (fd_in < 0) {
	    fprintf(stderr, "Failed to open input file\n");
	    close(sockfd);
	    exit(1);
	}

	bool flag = false;

	int n = 0;
	int window_seq = 0;
	int client_seq = 0;

	char data_pkt[mss];
	char buffer[mss-HEADER];

	//printf("Entering while loop\n");
	while(1)
	{
	    if(window_seq < winsz)
		n = read(fd_in, buffer, mss-HEADER);

	    //printf("File position: %ld\n", lseek(fd_in, 0, SEEK_CUR));

	    if(n <= 0)
		flag = true;

	    if(window_seq < winsz && !flag)
	    {
		code = 'D';
		memcpy(data_pkt, &code, sizeof(char));
		memcpy(data_pkt + sizeof(char), &id, sizeof(int));
		memcpy(data_pkt + sizeof(char) + sizeof(int), &client_seq, sizeof(int));
		memcpy(data_pkt + sizeof(char) + sizeof(int) + sizeof(int), &buffer, n);

		bytes_read[window_seq] = n;

		timeLog(code, client_seq, client_seq - window_seq, winsz, server_ip, server_port);

		if(sendto(sockfd, data_pkt, HEADER + n, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
		{
		    close(fd_in);
		    close(sockfd);
		    errno("Packet Failed to Send", 1);
		}

		usleep(10);

		window_seq++;
		client_seq++;
		continue;
	    }
	    else
	    {
		if(flag)
		{
		    char eof_pkt[HEADER];

		    code = 'E';
		    memcpy(eof_pkt, &code, sizeof(char));
		    memcpy(eof_pkt + sizeof(char), &id, sizeof(int));
		    memcpy(eof_pkt + sizeof(char) + sizeof(int), &client_seq, sizeof(int));

		    timeAckLog(code, client_seq, server_port, server_ip, false);

		    if(sendto(sockfd, eof_pkt, sizeof(eof_pkt), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
		    {
			close(fd_in);
			close(sockfd);
			errno("Packet Failed to Send", 1);
		    }
		}

		bool calm = false;
		int server_seq = 0;
		int retries = 0;
		char recv_ack_buffer[sizeof(char) + sizeof(int)];
		while(retries <= MAX_RETRIES)
		{
		    if(retries == MAX_RETRIES)
		    {
			close(fd_in);
			close(sockfd);
			if(calm)
			{
			    cleanup_processes();
			    exit(0);
			}
			else
			{
			    char err_msg[1024];
			    sprintf(err_msg, "%s %s", "Reached max re-transmission limit IP", server_ip);
			    sleep(1);
			    errno(err_msg, 4);
			}
		    }

		    fd_set read_fd;
		    struct timeval timeout;
		    timeout.tv_sec = 3;
		    timeout.tv_usec = 0;

		    FD_ZERO(&read_fd);
		    FD_SET(sockfd, &read_fd);

		    int retval = select(sockfd + 1, &read_fd, NULL, NULL, &timeout);

		    if(retval == -1)
		    {
			close(fd_in);
			close(sockfd);
			errno("Timeout Sequence Failed", 1);
		    }
		    else if(retval == 0)
		    {
			if(flag)
			{
			    char eof_pkt[HEADER];

			    code = 'E';
			    memcpy(eof_pkt, &code, sizeof(char));
			    memcpy(eof_pkt + sizeof(char), &id, sizeof(int));       
			    memcpy(eof_pkt + sizeof(char) + sizeof(int), &client_seq, sizeof(int));

			    timeAckLog(code, client_seq, server_ip, server_port, true);
			    
			    fprintf(stderr, "Packet loss detected\n");
			    if(sendto(sockfd, eof_pkt, sizeof(eof_pkt), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
			    {
				close(fd_in);
				close(sockfd);
				errno("EOF Packet Failed to Send", 1);
			    }
			    retries++;
			    calm = true;
			}
			else
			{
			    fprintf(stderr, "Packet loss detected\n");

			    timeAckLog(code, client_seq, server_ip, server_port, true);

			    if(sendto(sockfd, data_pkt, sizeof(data_pkt), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
			    {
				close(fd_in);
				close(sockfd);
				errno("Data Packet Failed to Send", 1);
			    }
			}
			retries++;
			continue;
		    }

		    ssize_t recv_len = recvfrom(sockfd, recv_ack_buffer, sizeof(recv_ack_buffer), 0, (struct sockaddr*)&server_addr, &serverLength);

		    if(recv_len != sizeof(char) + sizeof(int))
		    {
			retries++;
			continue;
		    }

		    if(code < 'A' || code > 'Z')
		    {                                   
			char err_msg[1024]; 
			sprintf(err_msg, "%s %s %s %s", "Server is down IP", server_ip, "port", server_port);    
			sleep(1);    
			errno(err_msg, 5);
		    }	

		    memcpy(&code, recv_ack_buffer, sizeof(char));
		    memcpy(&server_seq, recv_ack_buffer + sizeof(char), sizeof(int));

		    if(code == 'F')
		    {

			cleanup_processes();
			close(fd_in);
			close(sockfd);
			return 0;
		    }

		    while(server_seq < client_seq)
		    {
			lseek(fd_in, -bytes_read[window_seq-1], SEEK_CUR);
			client_seq--;
			window_seq--;
		    }

		    code = 'D';
		    timeAckLog(code, client_seq, server_port, server_ip, true);

		    window_seq = 0;

		    flag = false;

		    break;
		}
		continue;
	    }
	}

	close(fd_in);
	close(sockfd);
	exit(0);
    } 
    else 
    {
	child_pids[num_children++] = client_pid;
    }

    return 1;
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

void handle_child_signal(int sig) {
    (void) sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
	for (int i = 0; i < num_children; i++)
       	{
            if (child_pids[i] == pid)
	    {
                child_pids[i] = child_pids[num_children - 1];
                num_children--;
                break;
            }
        }
    }
}

void init_process_management() {
    
    struct sigaction sa;
    sa.sa_handler = handle_child_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) 
    {
        errno("Failed to set up signal handler", 1);
    }

    child_pids = malloc(sizeof(pid_t) * MAXLINE);
    if (!child_pids)
    {
        errno("Failed to allocate memory for child tracking", 1);
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

void cleanup_processes() {
    for (int i = 0; i < num_children; i++) 
    {
        if (child_pids[i] > 0)
       	{
            kill(child_pids[i], SIGTERM);
        }
    }

    int remaining = num_children;
    time_t start_time = time(NULL);
    
    while (remaining > 0 && time(NULL) - start_time < 5) 
    { 
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        
        if (pid > 0)
       	{
            for (int i = 0; i < num_children; i++) 
	    {
                if (child_pids[i] == pid)
	       	{
                    child_pids[i] = child_pids[num_children - 1];
                    num_children--;
                    remaining--;
                    break;
                }
            }
        } 
	else 
	{
            usleep(100000);  
        }
    }
    
    for (int i = 0; i < num_children; i++)
    {
        if (child_pids[i] > 0)
       	{
            kill(child_pids[i], SIGKILL);
        }
    }

    free(child_pids);
    child_pids = NULL;
}
void signal_handler(int sig) 
{
    (void) sig;
    cleanup_processes();
    exit(1);
}
