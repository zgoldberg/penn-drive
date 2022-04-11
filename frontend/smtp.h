#ifndef SMTP_H
#define SMTP_H

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

#define MAX_NUM_CONNECTIONS_SMTP_SERVER 100
#define BUFFER_SIZE_SMTP_SERVER 100000


struct mail_state_smtp {
  string domain;
  string mail_from;
  vector<string> rcpt_tos;
  bool recieving_data;
  string data;
};


void sig_handler_smtp(int sig);
void parse_args_smtp(int argc, char** argv, int* port_number) ;
int read_message_smtp(int fd, char* buffer);
int write_message_smtp(int fd, char* buffer);
bool string_in_vector_smtp(string s, vector<string>* v);
void* smtp_thread_handle_client(void* argument_pointer);
void* run_smtp_server(void* port_number);

#endif
