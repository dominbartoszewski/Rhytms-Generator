#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#define MAX_LINE_SIZE 256

static char *rand_string(char *str, size_t size);
char* rand_string_alloc(size_t size);
void sigHandler(int sigNum, siginfo_t *si, void *pVoid);
void sig_func();
void parse_args(int,char **);
int print_help_exit( char* );
int decode_signal_data(int sig_int);
long count_bits(char *file_name);
int open_file(char *file_name, int flag);
void prepare_info_and_sig_send(int client_pid, int accept_code, int number_characters, short int offset );
void err_Exit(char *msg);
int count_files(char *catalog);
void allocate_memory(char *catalog);
char *make_line( char *mr_tadeusz, int *character, long size);
char *make_word( char *mr_tadeusz, int *character, long size);
char *make_letter( char *mr_tadeusz, int *character, long size);
void send_line(int *index_line);
void send_word(int* index_word);
void send_letters(int *index_letter);
void receive_action(int *index_line, int *index_word, int *index_letter);

int sock;
struct sockaddr_un name;
struct sockaddr_un cli_name;
int rt_number;
int chapter_number;
int interval;
char fragment_text;
char *notice_path;
char catalog[255];

time_t seconds = 0;
long int nanoseconds = 15625000L;

struct timespec tc;
int number_chapters;
int count_clients = 0;

char **national_epic;
long *chapter_length;

const long int NANO = 1000000000L;

union converter
{
	unsigned char bytes[4];
	int code_int;
};

int main(int argc, char* argv[])
{
	if( argc < 5)
		print_help_exit(argv[0]);

	srand(time(NULL));
	parse_args(argc,argv);
	sig_func();
	allocate_memory(catalog);

	printf("PID: %d \n", getpid());
	
	int index_line = 0;
	int index_word = 0;
	int index_letter = 0;

	while(1)
	{	
		pause();

		if( count_clients > 0 )
		{
			pid_t child_pid;
			child_pid = fork();
			
			if( child_pid == 0 )
			{
				while(1)
				{
					receive_action(&index_line, &index_word, &index_letter);
				}
			}
		}
	}
}	

void receive_action(int *index_line, int *index_word, int *index_letter)
{
	if( fragment_text == 'l')
		send_line(index_line);

	if( fragment_text == 's')
		send_word(index_word);

	if( fragment_text == 'z')
		send_letters(index_letter);

	char server_buffer[MAX_LINE_SIZE];
	memset(server_buffer, 0, sizeof(server_buffer));

	socklen_t len = sizeof( struct sockaddr_un );			
	if( recvfrom( sock, server_buffer, sizeof(server_buffer), 0, (struct sockaddr *)&name, &len) == -1 )
	{
		unlink(cli_name.sun_path);
		close(sock);
		err_Exit("reading from client failed! \n");
	}
	
	int i = server_buffer[0] - '0';
	if( i == -48 )
	{
		unlink(cli_name.sun_path);
		close(sock);
	}

}

void send_line(int *index_line)
{
	char *line = make_line( national_epic[chapter_number-1], index_line, chapter_length[chapter_number-1]);

	if( sendto( sock, line, strlen(line), 0,
				(struct sockaddr *)&cli_name,
				sizeof(struct sockaddr_un)) < 0)
	{
		close(sock);
		unlink(cli_name.sun_path);
		count_clients--;
		err_Exit("Client disconnected! \n");
	}

	long int time_sleep = nanoseconds * interval;
	struct timespec tc = { (time_t)(time_sleep/NANO), (long int)(time_sleep % NANO) };

	nanosleep(&tc, NULL);
}

void send_word(int* index_word)
{
	char *word = make_word( national_epic[chapter_number-1], index_word, chapter_length[chapter_number-1]);

	if( sendto( sock, word, strlen(word), 0,
				(struct sockaddr *)&cli_name,
				sizeof(struct sockaddr_un)) < 0)
	{
		close(sock);
		unlink(cli_name.sun_path);
		count_clients--;
		err_Exit("Client disconnected! \n");
	}

	long int time_sleep = nanoseconds * interval;
	struct timespec tc = { (time_t)(time_sleep/NANO), (long int)(time_sleep % NANO) };
	nanosleep(&tc, NULL);
}


void send_letters(int *index_letter)
{
	char *letter = make_letter( national_epic[chapter_number-1], index_letter, chapter_length[chapter_number-1]);

	if( sendto( sock, letter, strlen(letter), 0,
				(struct sockaddr *)&cli_name,
				sizeof(struct sockaddr_un)) < 0)
	{
		close(sock);
		unlink(cli_name.sun_path);
		count_clients--;
		err_Exit("Client disconnected! \n");
	}
	
	long int time_sleep = nanoseconds * interval;
	struct timespec tc = { (time_t)(time_sleep/NANO), (long int)(time_sleep % NANO) };
	nanosleep(&tc, NULL);
}

void sigHandler(int sigNum, siginfo_t *si, void *pVoid)
{
	if(sigNum == SIGRTMIN + 11)
	{
		int accept_code = decode_signal_data(si->si_value.sival_int);	

		if( accept_code == 1 )
		{
			pid_t client_pid = si->si_pid;
			sock = socket(AF_UNIX, SOCK_DGRAM, 0);

			if (sock < 0) {
				err_Exit("sock() error! \n");
			}
			
			char *random_string = rand_string_alloc(32);		
			memset(&name, 0, sizeof( struct sockaddr_un));

			name.sun_family = AF_UNIX;
			strncpy(&name.sun_path[1], random_string, sizeof(name.sun_path) -1);	
			name.sun_path[0] = 0;

			if( bind(sock, (struct sockaddr *) &name, sizeof(struct sockaddr_un)))
			{
				err_Exit("bind() for client error! \n");
			}

			int fd = open(notice_path, O_RDWR|O_CREAT, S_IRWXU );
			long notice_board_size = count_bits(notice_path);

			int offset = lseek( fd, notice_board_size, SEEK_SET);
			ssize_t write_status = write( fd, random_string, strlen(random_string));
			close(fd);

			prepare_info_and_sig_send(client_pid, accept_code, 
					strlen(random_string), (short)(offset + write_status));	
			
			count_clients++;
			
			socklen_t len = sizeof( struct sockaddr_un );
			
			char buf[8];	
			if( recvfrom( sock, buf, sizeof(buf), 0, (struct sockaddr *)&cli_name, &len) == -1 )
			{
				err_Exit("Reading test message from client failed! \n");
			}

			write(STDOUT_FILENO, "Client connected! \n", 20);
		}
		else
		{	
			prepare_info_and_sig_send(si->si_pid, accept_code, 0, 0);
		} 
	}
}

void sig_func()
{
	struct sigaction sa;
	memset( &sa, '\0', sizeof(sa));
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sigHandler;

	if( sigaction(SIGRTMIN+11, &sa, NULL) == -1 )
	{
		err_Exit("Sigaction function error! \n");
	}
}

int decode_signal_data(int sig_int)
{
	int accept_code;
	unsigned char bytes[4];
	bytes[3] = (sig_int >> 24) & 0xFF;
	bytes[2] = (sig_int >> 16) & 0xFF;
	bytes[1] = (sig_int >> 8) & 0xFF;
	bytes[0] = sig_int & 0xFF;

	rt_number = (pid_t)bytes[3];
	chapter_number = (int)bytes[2];
	fragment_text = (char)bytes[1];
	interval = (int)bytes[0];

	if( chapter_number < 1 
			|| chapter_number > 12
			|| SIGRTMIN + rt_number > SIGRTMAX 
			|| interval < 1)
		accept_code = 0;
	else
		accept_code = 1;

	return accept_code;		
}

void prepare_info_and_sig_send(int client_pid, int accept_code, int number_characters, short int offset )
{
	unsigned char bytes[4];

	bytes[3] = accept_code;
	bytes[2] = number_characters;
	bytes[1] = (offset >> 8) & 0xFF;
	bytes[0] = offset & 0xFF;

	union converter conv;

	conv.bytes[3] = bytes[3];
	conv.bytes[2] = bytes[2];
	conv.bytes[1] = bytes[1];
	conv.bytes[0] = bytes[0];

	int code_int = conv.code_int;

	union sigval sv;
	sv.sival_int = code_int;
	sigqueue(client_pid, SIGRTMIN + rt_number, sv);
}

long count_bits(char *file_name)
{
	struct stat st;
	int status = stat(file_name, &st);
	return st.st_size;
}

int open_file(char *file_name, int flag)
{
	int fd = open(file_name, flag);
	if( fd < 0 )
	{
		err_Exit("Open file error! \n");
		return -1;
	}
	return fd;
}


static char *rand_string(char *str, size_t size)
{
	const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPRSTUWXYZ";
	if (size) 
	{
		--size;
		
		for (size_t n = 0; n < size; n++)
		{
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
	if (s) 
	{
		rand_string(s, size);
	}
	return s;
}

void parse_args(int argc, char **argv)
{
	int c;
	const char *short_opt = "k:p:";
	notice_path = (char *)malloc(255);
	while((c = getopt(argc, argv, short_opt)) != -1)
	{
		switch(c) {
			case 'k':
				strcpy( catalog, optarg);
				break;

			case 'p':
				notice_path = strcpy( notice_path, optarg );
				break;
			case ':':
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
	printf("Usage: %s -k <PATH> -p <PATH> \n", name);
	printf("\n");
	exit(-2);
}

void err_Exit(char *msg)
{
	write(STDERR_FILENO, msg, strlen(msg));
	_exit(-1);
}

void allocate_memory(char *catalog)
{
	number_chapters = count_files(catalog);
	national_epic = (char**)malloc(number_chapters * sizeof(char*));
	chapter_length = (long *)malloc(number_chapters * sizeof(long));

	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir (catalog)) != NULL) 
	{
		while ((ent = readdir (dir)) != NULL)
		{
			if( strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
				continue;

			char temppath[255];
			memset(temppath,0,sizeof(temppath));
			strcpy( temppath, catalog );
			strcpy( temppath + strlen( catalog ), "/");
			strcpy( temppath + strlen( catalog ) + 1, ent->d_name );
			long size = count_bits(temppath);
			int row = atoi(ent->d_name)-1;

			national_epic[row] = (char*)malloc(size+1);
			chapter_length[row] = size;

			int fd = open_file(temppath, O_RDONLY);
			char *buffer = (char *)malloc(1);
			ssize_t read_status;
			lseek(fd, SEEK_SET, 0);

			int j = 0;
			while( ( read_status = read( fd, buffer, 1) ) != 0 )
			{
				national_epic[row][j] = buffer[0];
				j++;
			}

			lseek(fd, SEEK_SET,0);
			close(fd);
		}
		closedir (dir);
	} 
	else
	{
		err_Exit("There's no directory! \n");	
	}
}

int count_files(char *catalog)
{
	DIR *dir;
	struct dirent *ent;
	int number_chapters = 0;

	if ((dir = opendir(catalog)) != NULL) 
	{
		while ((ent = readdir(dir)) != NULL) 
		{
			if( strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
				continue;

			number_chapters++;
		}
		closedir (dir);
	} 
	else
	{
		err_Exit("There's no directory! \n");	
	}

	return number_chapters;
}

char *make_line( char *mr_tadeusz, int *character, long size)
{
	char *line = (char *)malloc( MAX_LINE_SIZE * sizeof(char));
	memset(line, 0, MAX_LINE_SIZE);

	for( int i = *character, j = 0 ; i<size; i++)
	{
		line[j++] = mr_tadeusz[i];
		(*character)++;

		if( mr_tadeusz[i] == '\n' 
				|| mr_tadeusz[i] == '\0'
				|| j == MAX_LINE_SIZE )
		{
			line[j] = '\0';
			break;	
		}
	}

	return line;
}

char *make_word( char *mr_tadeusz, int *character, long size)
{
	char *line = (char *)malloc( MAX_LINE_SIZE *  sizeof(char));
	memset(line, 0, MAX_LINE_SIZE);
	
	for( int i = *character, j = 0 ; i<size; i++)
	{
		line[j++] = mr_tadeusz[i];
		(*character)++;
		if( mr_tadeusz[i] == ' ' 
				|| mr_tadeusz[i] == '\0' 
				|| mr_tadeusz[i] == '\t'
				|| mr_tadeusz[i] == '\n'
				|| j == MAX_LINE_SIZE )
		{
			line[j] = '\0';
			break;	
		}
	}

	return line;
}

char *make_letter( char *mr_tadeusz, int *character, long size)
{
	char *letter = (char *)malloc( 2 * sizeof(char));
	memset(letter, 0, 2);

	letter[0] = mr_tadeusz[(*character)++];
	letter[1] = 0;
	
	return letter;
}

