#define _GNU_SOURCE 
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#define NANO 1000000000L
#define CONF "conf.txt"
#define WIDTH 128
#define LENGTH 128
#define SIZE_OF_ALPHABET 27

char **fifo_path_tab;
int size_fifo_paths;
long size_conf_file;

char config_buffer[WIDTH][LENGTH];
char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

int alphabet_index = 0;
int alphabet_index1 = 0;
int alphabet_index2 = 0;

float delay;

timer_t timerid_usr1, timerid_usr2;

struct sigevent sev_usr1, sev_usr2;
struct itimerspec its_usr, its_disarm;

int all_rows;

int num_line_rt1 = -1;
int num_column_rt1 = 0;
int num_line_rt2 = -1;
int num_column_rt2 = 0;

int canal;
int canal_ready = 0;

int pipe_usr1[2];
int pipe_usr2[2];

int count_usr1 = 0;
int count_usr2 = 0;

struct line_and_index {
	int line;
	int index;
};

struct temp_str {
	int line;
	int index;
};

struct temp_str ts;
struct timespec timsp;

void err_Exit(char *msg)
{
	write(STDERR_FILENO, msg, strlen(msg));
	_exit(-1);
}

void clear_screen()
{
	printf("\e[1;1H\e[2J");
}

int load_conf_file()
{
	FILE *ptr_file = NULL; 
	int index = 0;
	int sum = 0;
	ptr_file = fopen(CONF, "r");
	
	if(!ptr_file)
	{
		err_Exit("Unable to open config file! \n");
	}

	while(fgets(config_buffer[index], LENGTH , ptr_file )) 
	{
		config_buffer[index][strlen(config_buffer[index]) - 1] = '\0';
		index++;
	}
	
	sum = index;
	
	return sum;
}

int print_help_exit(char *name)
{
	printf("Usage: %s [OPTIONS]\n", name);
	printf("  -d <float>, --delay=<float> <PATH>  	delay and path to FIFO \n");
	printf("  -h, --help  				Print help \n");
	printf("\n");
	return(-1);
}

void parse_args(int argc, char **argv)
{
	int c;
	const char *short_opt = "hd:";
	char* strtof_end;
	
	struct option long_opt[] =
	{
		{ "help",	no_argument,       NULL, 'h'},
		{ "delay",	required_argument, NULL, 'd'},
		{ NULL,		0,                 NULL,  0 }
	};

	while((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch(c) {
			case 'd':
				delay = strtof(optarg, &strtof_end);		
				break;

			case 'h':
				print_help_exit(argv[0]);
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
	
	size_fifo_paths = argc - optind;
	fifo_path_tab = (char **)malloc (size_fifo_paths * sizeof (char *));
	
	for( int i=0; i<size_fifo_paths; i++)
	{
		fifo_path_tab[i] = (char *)malloc (128 * sizeof (char));
	}

	if (optind < argc) {
		int index_tab_path = 0;
		while (optind < argc)
		{
			fifo_path_tab[index_tab_path++] = argv[optind++];
		}
	}
	else
	{	
		err_Exit("CRITICAL ERROR! Paths are not specified! \n");

	}
}

void make_timer()
{
	sev_usr1.sigev_notify = sev_usr2.sigev_notify = SIGEV_SIGNAL;
	sev_usr1.sigev_signo = SIGRTMIN;
	sev_usr1.sigev_value.sival_ptr = &timerid_usr1;

	its_usr.it_value.tv_sec = (long)delay;
	its_usr.it_value.tv_nsec = ( delay - its_usr.it_value.tv_sec ) * NANO;
	its_usr.it_interval.tv_sec = its_usr.it_value.tv_sec;
	its_usr.it_interval.tv_nsec = its_usr.it_value.tv_nsec;
	
	sev_usr2.sigev_signo = SIGRTMIN+1;
	sev_usr2.sigev_value.sival_ptr = &timerid_usr2;

	its_disarm.it_value.tv_sec = its_disarm.it_value.tv_nsec = 0;
	its_disarm.it_interval.tv_sec = its_disarm.it_interval.tv_nsec = 0;
	
	if((timer_create(CLOCK_MONOTONIC, &sev_usr1, &timerid_usr1) == -1)
		|| (timer_create(CLOCK_MONOTONIC, &sev_usr2, &timerid_usr2) == -1))
	{
		err_Exit("CRITICAL ERROR! Unable to make timer! \n");
	}
}

void set_timer(timer_t timerid, struct itimerspec its)
{
	int ret_timer = timer_settime(timerid, 0, &its, NULL);
	if( ret_timer == -1 )
		err_Exit("CRITICAL ERROR! Unable to set timer! \n");
}

void make_pipes()
{
	if( (pipe2(pipe_usr1, O_NONBLOCK) == -1 ) || (pipe2(pipe_usr2, O_NONBLOCK) == -1))
		err_Exit("CRITICAL ERROR! Unable to make needed self pipe trick! \n");
}


void sigusr1_handler()
{
	struct line_and_index lai; 

	lai.line = (( num_line_rt1 +1  ) % all_rows); 
	lai.index = ((alphabet_index1 +1 ) % 27) ; 

	if( write(pipe_usr1[1], &lai, sizeof(lai)) < 1)
		err_Exit("CRITICAL ERROR! Unable to write to pipe[1]! \n"); 
}

void sigusr2_handler()
{
	struct line_and_index lai;
		
	lai.line = (( num_line_rt2 + 1 ) % all_rows);
	lai.index = ((alphabet_index2 +1 ) % 27) ;

	if( write(pipe_usr2[1], &lai, sizeof(lai)) < 1)
		err_Exit("CRITICAL ERROR! Unable to write to pipe[2]! \n");
}

void sigrt1_handler()
{
	if(num_line_rt1 == all_rows)
		num_line_rt1 = 0;

	if( config_buffer[num_line_rt1][num_column_rt1] != '\0' ) 
	{
		if( config_buffer[num_line_rt1][num_column_rt1] == '*' )
		{
			if( canal_ready )
			{
				if( write(canal, &alphabet[(alphabet_index1-1)], 1) > 0)
					num_column_rt1++;
				else
				{
					canal_ready = 0;
					close(canal);
				}
			}
		}
		else
			num_column_rt1++;
	}
	else
	{
		count_usr1--;
		num_column_rt1 = 0;
		timer_settime(timerid_usr1, 0, &its_disarm, NULL); 
	}
}

void sigrt2_handler()
{
	if( num_line_rt2 == all_rows)
		num_line_rt2 = 0;

	if( config_buffer[num_line_rt2][num_column_rt2] != '\0' )
	{
		if( config_buffer[num_line_rt2][num_column_rt2] == '*' )
		{
			if( canal_ready )
			{
				if( write(canal, &alphabet[(alphabet_index2-1)], 1) > 0)
					num_column_rt2++;
				else
				{
					canal_ready = 0;
					close(canal);
				}
			}
		}
		else
			num_column_rt2++;
	}
	else
	{
		count_usr2--;
		num_column_rt2 = 0;
		timer_settime(timerid_usr2, 0, &its_disarm, NULL); 
	}
}

void sigHandler( int sigNum, siginfo_t *si, void *pVoid)
{
	if( sigNum == SIGUSR1 )
		sigusr1_handler();
	
	if( sigNum == SIGUSR2 )
		sigusr2_handler();
	
	if( sigNum == SIGRTMIN )
		sigrt1_handler();	

	if( sigNum == SIGRTMIN +1 )	
		sigrt2_handler();

	if ( sigNum == SIGPIPE )
	{

	}
}

void sig_func()
{
	struct sigaction sa;
	memset( &sa, '\0', sizeof(sa));
	sa.sa_flags = SA_RESTART;
	sa.sa_sigaction = sigHandler;

	if( sigaction(SIGUSR1, &sa, NULL) == -1 
			|| sigaction(SIGUSR2, &sa, NULL) == -1
			|| sigaction(SIGRTMIN, &sa, NULL) == -1
			|| sigaction(SIGRTMIN+1, &sa, NULL) == -1
			|| sigaction(SIGPIPE, &sa, NULL) == -1 )
	{
		err_Exit("Sigaction() error! \n");
	}

}

long count_bits()
{
	struct stat st;
	int status = stat(CONF, &st);
        return st.st_size;
}

int search_canal(char **fifo_path_tab, int size_fifo_paths)
{
	timsp.tv_sec = (time_t)1;
	timsp.tv_nsec = (long)0;

	for(;;)
	{
		for( int i=0; i<size_fifo_paths; i++)
		{
			struct stat sb;
			if( !(stat(fifo_path_tab[i], &sb) ))
			{
				if((sb.st_mode & S_IFMT) == S_IFIFO)
				{
					int fd_write = open(fifo_path_tab[i], O_WRONLY | O_NONBLOCK);
					printf("Searching communication canal... \n");
					clear_screen();

					if( fd_write > 0 )
					{
						printf("Communication canal %s is ready to receive data. \n", fifo_path_tab[i]);
						return fd_write;
					}
					else
						continue;

				}
			}
		}
		nanosleep( &timsp, NULL );
	}
	return -1;
}

void try_to_search_canal()
{
	if(!canal_ready)
	{
		canal = search_canal(fifo_path_tab, size_fifo_paths);
              	canal_ready = 1;
        }
}

void wait_for_signals()
{
	struct timespec tsp;
	tsp.tv_sec = (time_t)1;
	tsp.tv_nsec = (long)0;

	while( !count_usr1 || !count_usr2 )
	{
		if(!count_usr1)
		{
			if( (read(pipe_usr1[0], &ts, sizeof(ts) )) > 0)
			{
				count_usr1++;
				num_line_rt1 = ts.line;
				alphabet_index1 = ts.index;
				set_timer(timerid_usr1, its_usr);
				break;
			}
		}

		if(!count_usr2)
		{
			if( (read(pipe_usr2[0], &ts, sizeof(ts) )) > 0)
			{
				count_usr2++;
				num_line_rt2 = ts.line;
				alphabet_index2 = ts.index;
				set_timer(timerid_usr2, its_usr);
				break;
			}
		}		

		try_to_search_canal();
		nanosleep(&tsp, NULL);
	}
	try_to_search_canal();
}

int main(int argc, char * argv[])
{
	if(argc < 3)
		print_help_exit(argv[0]);
	
	parse_args(argc, argv);
	make_pipes();
	make_timer();

	sig_func();

	size_conf_file = count_bits();
	all_rows = load_conf_file();

	ts.line = 0;
	ts.index = 0;
	canal_ready = 0;
	
	try_to_search_canal();
	
	for(;;)
	{
		wait_for_signals();		
	}

	return(0);
}
