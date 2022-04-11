#ifndef POP3_H
#define POP3_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>
#include <sys/file.h>

#include <vector>
#include <string>
#include <algorithm>

#include "mail.h"

#define MAX_NUM_CONNECTIONS_POP3_SERVER 100
#define BUFFER_SIZE_POP3_SERVER 100000


// structure to keep track of mail state
struct mail_state_pop3 {
	string user;
	char state;
};

void clear_mail_state_pop3(struct mail_state_pop3* ms);
void sig_handler_pop3(int sig);
void parse_args_pop3(int argc, char** argv, int* port_number);
int read_message(int fd, char* buffer);
int write_message(int fd, char* buffer);
void multi_line_write(int fd, char* string);
int handle_message(char* message, int fd, struct mail_state_pop3* ms, vector<string>* UIDLs_to_delete);
void* pop3_thread_handle_client(void* argument_pointer);
void* run_pop3_server(void* port_number_arg);

#endif
