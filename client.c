#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#define MAX_LINE_SIZE 256

//=====================
void sigHandler(int sigNum, siginfo_t *si, void *pVoid);
void sig_func();
void parse_args(int , char **);
void err_Exit(char *msg);
void prepare_info_and_sig_send();
int print_help_exit(char *);
int decode_signal_data(int sig_int);
int open_file(char *file_name, int flag);
char* rand_string_alloc(size_t size);
static char *rand_string(char *str, size_t size);
void make_own_socket();
char *rot13_cipher(char *string);
//=====================
int confirmed = 0;
int sock;
struct sockaddr_un serv_name;
struct sockaddr_un own_name;

pid_t server_pid;
int rt_number;
int chapter_number;
int interval;
int ready_to_receive = 0;
int acceptation;
int number_characters;
int offset;
char fragment_text;
char notice_path[255];

time_t second = 0;
long nanosecond = 300000000;

struct timespec tc;

union converter {
	unsigned char byte[4];
	int code_number;
};

//=====================
int main( int argc, char *argv[])
{
	if( argc < 11)
		print_help_exit(argv[0]);

	parse_args(argc,argv);	

	sig_func();
	prepare_info_and_sig_send();

	if( confirmed == 0 )	
	{
		pause();
	}
	
	make_own_socket();

	char my_pid[8];
	sprintf(my_pid, "%d", getpid());

	if( sendto( sock, my_pid, sizeof(my_pid), 0, 
				(struct sockaddr *)&serv_name, 
				sizeof(struct sockaddr_un)) < 0)
	{
		err_Exit("Send test client to server failed! \n");
	}

	while(1)
	{
		char client_buffer[MAX_LINE_SIZE];
		memset(client_buffer,0, sizeof(client_buffer));
		socklen_t len = sizeof( struct sockaddr_un );

		if( recvfrom( sock, client_buffer, sizeof(client_buffer), 0, (struct sockaddr *)&own_name, &len) == -1 )
		{
			err_Exit("Reading from server failed! \n");
		}

		write(STDOUT_FILENO, client_buffer, sizeof(client_buffer)); 

		if( sendto( sock, rot13_cipher(client_buffer), sizeof(client_buffer), 0, 
					(struct sockaddr *)&serv_name, 
					sizeof(struct sockaddr_un)) < 0)
		{
			err_Exit("Sending to server failed! \n");
		}
	}

	close(sock);
}

void sigHandler(int sigNum, siginfo_t *si, void *pVoid)
{
	if(sigNum == SIGRTMIN + rt_number)
	{
		int accept_code = decode_signal_data(si->si_value.sival_int);

		if( accept_code == 0 )
		{
			err_Exit("Server not accepted! Maybe you gave wrong parameters! \n");
		}	

		char *adress_buffer = (char *)malloc(number_characters+1);

		int fd = open_file( notice_path, O_RDONLY);
		lseek( fd, offset-number_characters, SEEK_SET);
		read(fd, adress_buffer, number_characters);

		close(fd);     

		memset(&serv_name, 0, sizeof( struct sockaddr_un));
		serv_name.sun_family = AF_UNIX;
		strncpy(&serv_name.sun_path[1], adress_buffer, sizeof(serv_name.sun_path) -1 );

		confirmed = 1;
	}
}

void sig_func()
{
	struct sigaction sa;
	memset( &sa, '\0', sizeof(sa));
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sigHandler;

	if( sigaction(SIGRTMIN + rt_number, &sa, NULL) == -1  )
	{
		err_Exit("Sigaction() function error! \n");
	}
}

void make_own_socket()
{
	sock = socket(AF_UNIX, SOCK_DGRAM, 0);

	if( sock < 0)
	{
		err_Exit("Error making own socket! \n");
	}

	srand(time(NULL));

	char *random_string = rand_string_alloc(32);
	memset(&own_name, 0, sizeof( struct sockaddr_un ));
	own_name.sun_family = AF_UNIX;

	strncpy(&own_name.sun_path[1], random_string, sizeof(own_name.sun_path) -1);    
	own_name.sun_path[0] = 0;

	if( bind( sock, (struct sockaddr *)&own_name, sizeof(struct sockaddr_un)))
	{
		err_Exit("bind() own socket error! \n");
	}
}

void prepare_info_and_sig_send()
{
	unsigned char byte_1, byte_2, byte_3, byte_4;

	byte_1 = rt_number;
	byte_2 = chapter_number;
	byte_3 = fragment_text;
	byte_4 = interval;

	union converter c;

	c.byte[3] = byte_1;
	c.byte[2] = byte_2;
	c.byte[1] = byte_3;
	c.byte[0] = byte_4;

	int r = c.code_number;
	union sigval sv;
	sv.sival_int = r;
	int send_status = sigqueue(server_pid, SIGRTMIN+11, sv);

	if( send_status == -1)
	{
		err_Exit("Sigqueue() error! Maybe you gave wrong server PID! \n");
	}	

}

int decode_signal_data(int sig_int)
{
	unsigned char bytes[4];
	bytes[3] = (sig_int >> 24) & 0xFF;
	bytes[2] = (sig_int >> 16) & 0xFF;
	bytes[1] = (sig_int >> 8) & 0xFF;
	bytes[0] = sig_int & 0xFF;

	acceptation = (int)bytes[3];
	number_characters = (int)bytes[2];
	offset = (int)bytes[1] << 8 | bytes[0];
	return acceptation;
}

static char *rand_string(char *str, size_t size)
{
	const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPRSTUWXYZ";
	if (size) {
		--size;
		for (size_t n = 0; n < size; n++) {
			int key = rand() % (int) (sizeof charset - 1);
			str[n] = charset[key];
		}
		str[size] = '\0';
	}
	return str;
}

char* rand_string_alloc(size_t size)
{
	char *s = malloc(size + 1);
	if (s) {
		rand_string(s, size);
	}
	return s;
}

void parse_args(int argc, char **argv)
{
	int c;
	const char *short_opt = "s:r:x:o:f:p:";
	char *p_end_s, *p_end_r, *p_end_x, *p_end_p;

	while((c = getopt(argc, argv, short_opt)) != -1)
	{
		switch(c) {
			case 's':
				server_pid = (pid_t)strtol(optarg, &p_end_s, 10);
				if( server_pid < 0)
					err_Exit("Podano zly pid! \n");
				break;
			case 'r':
				rt_number = (int)strtol(optarg, &p_end_r, 10);
				if( rt_number < 0)
					err_Exit("Zly numer sygnalu rt! \n");
				break;
			case 'x':
				chapter_number = (int)strtol(optarg, &p_end_x, 10);
				break;
			case 'o':
				interval = (int)strtol(optarg, &p_end_p, 10);
				break;
			case 'f':
				if( optarg[0] == 'l' || optarg[0] == 'z' || optarg[0] == 's')
				{
					fragment_text = optarg[0];
				}
				else
					err_Exit("Zly sposob fragmentacji! \n");

				break;
			case 'p':
				strncpy( notice_path, optarg, sizeof(notice_path) );
				break;
			case '?':
				fprintf(stderr, "Try `%s --help' for some help.\n", argv[0]);
				exit(-2);

			default:
				fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
				fprintf(stderr, "Try `%s --help' for some help.\n", argv[0]);
				exit(-2);
		};
	};
}

int print_help_exit(char *name)
{
	printf("Usage: %s -s <pid> -r <rt_signal_nr> -x <chapter nr> -p <noticeboard_path> -f <fragmentation> -o <interval>\n", name);
	printf("\n");
	exit(-2);
}

void err_Exit(char *msg)
{
	write(STDERR_FILENO, msg, strlen(msg));
	_exit(-1);
}

int open_file(char *file_name, int flag)
{
	int fd = open(file_name, flag);
	if( fd < 0 )
	{
		err_Exit("Cannot open noticeboard file! \n");
	}
	return fd;

}

char *rot13_cipher(char *string)
{	
	if (string == NULL)
	{
		return NULL;
	}


	for (int i = 0; string[i]; i++) 
	{
		if (string[i] >= 'a' && string[i] <= 'm') { string[i] += 13; continue; }
		if (string[i] >= 'A' && string[i] <= 'M') { string[i] += 13; continue; }
		if (string[i] >= 'n' && string[i] <= 'z') { string[i] -= 13; continue; }
		if (string[i] >= 'N' && string[i] <= 'Z') { string[i] -= 13; continue; }
	}

	return string;
}
