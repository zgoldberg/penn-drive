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
#include "smtp.h"

using namespace std;


extern volatile bool keep_running;
extern bool debug_mode;



//Signal handler for SIGINT so that program can exit gracefully
void sig_handler_smtp(int sig) {
	if (sig == SIGINT) {
		keep_running = false;
	}
}


// Function to parse command-line arguments and populate relevent variables
void parse_args_smtp(int argc, char** argv, int* port_number) {
	char c;
	*port_number = 8000; // default port number
	while ((c = getopt(argc, argv, "vp:")) != -1) {
		switch (c) {
      case 'v':
        debug_mode = true;
        break;
			case 'p':
				sscanf(optarg, "%d", port_number);
				break;
		}
	}
}


int read_message_smtp(int fd, char* buffer) {
	int read_result;
	for (int j = 0; j < BUFFER_SIZE_SMTP_SERVER; buffer[j++] = 0);

	for (int i = 0; i < BUFFER_SIZE_SMTP_SERVER; i++) {
		read_result = read(fd, buffer + i, 1);
		if (read_result == 0) {
			return 0;
		} else if (read_result == -1 && errno == 4) {
			return 2;
		} else if (i >= 1 && buffer[i - 1] == '\r' && buffer[i] == '\n') {
			buffer[i - 1] = 0;
			return 1;
		}
	}
	return -1;
}

int write_message_smtp(int fd, char* buffer) {
  int res = send(fd, buffer, strlen(buffer), 0);
  return res;
}


bool string_in_vector_smtp(string s, vector<string>* v) {
  for (unsigned long int i = 0; i < v->size(); i++) {
    if (v->at(i) == s) {
      return true;
    }
  }
  return false;
}


void* smtp_thread_handle_client(void* argument_pointer) {
  int client_fd = (long) argument_pointer;

  // KeyValueClient client(grpc::CreateChannel("localhost:5000", grpc::InsecureChannelCredentials()));

  // string hashes = list_email_hashes

  char recv_buffer[BUFFER_SIZE_SMTP_SERVER], send_buffer[BUFFER_SIZE_SMTP_SERVER];
  struct mail_state_smtp ms;
  bzero(&ms, sizeof(struct mail_state_smtp));

  if (debug_mode) {
    printf("[%d] New connection\n", client_fd);
  }

  sprintf(recv_buffer, "220 localhost +OK Server ready (Author: Zachary Goldberg / zachgold)\r\n");
  write_message_smtp(client_fd, recv_buffer);

  while (true) {
    bzero(recv_buffer, BUFFER_SIZE_SMTP_SERVER);
    bzero(send_buffer, BUFFER_SIZE_SMTP_SERVER);

    int read_value = read_message_smtp(client_fd, recv_buffer);

    if (read_value == 1 && debug_mode) {
      printf("[%d] C: %s\n", client_fd, recv_buffer);
    }

    if (read_value == 0) {
      if (debug_mode) {
        printf("[%d] Connection closed\n", client_fd);
      }
      close(client_fd);
      return 0;
    }

    if (!keep_running) {
      sprintf(send_buffer, "-ERR Server shutting down\r\n");
      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
      close(client_fd);
      return 0;
    }

    string recv_string(recv_buffer);

    if (ms.recieving_data) {
      if ((strlen(recv_buffer) == 1 && recv_buffer[0] == '.') || (strlen(recv_buffer) == 2 && recv_buffer[1] == '.') || (strlen(recv_buffer) == 3 && recv_buffer[2] == '.')) {
        sprintf(send_buffer, "250 OK\r\n");
        write_message_smtp(client_fd, send_buffer);
        if (debug_mode) {
          printf("[%d] S: %s", client_fd, send_buffer);
        }


        for (unsigned long int i = 0; i < ms.rcpt_tos.size(); i++) {
					MasterBackendClient master_client(
							grpc::CreateChannel(
									"localhost:5000",
									grpc::InsecureChannelCredentials()));

					string channel = master_client.which_node_call(ms.rcpt_tos.at(i));
					KeyValueClient client(
							grpc::CreateChannel(
									channel,
									grpc::InsecureChannelCredentials()));
          write_email(ms.rcpt_tos.at(i), ms.mail_from.c_str(), ms.data, client); // TODO add client
        }

        ms.recieving_data = false;
        bzero(&ms, sizeof(struct mail_state_smtp));

      }

      else {
        ms.data.append(recv_string);
        ms.data.append("\r\n");
      }
    }

    else if (!strcasecmp(recv_string.substr(0, 4).c_str(), "QUIT")) {
      sprintf(send_buffer, "221 +OK Goodbye!\r\n");
      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
      close(client_fd);
      return 0;
    }

    else if (!strcasecmp(recv_string.substr(0, 4).c_str(), "HELO")) {
      string domain = recv_string.substr(4, recv_string.length());
      ms.domain = domain;
      sprintf(send_buffer, "250 localhost\r\n");
      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
    }

    else if (!strcasecmp(recv_string.substr(0, 11).c_str(), "MAIL FROM:<")) {
      string mail_from = recv_string.substr(11, recv_string.length()-1);
      mail_from = mail_from.substr(0, mail_from.find(">"));
      ms.mail_from = mail_from;
      sprintf(send_buffer, "250 OK\r\n");
      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
    }

    else if (!strcasecmp(recv_string.substr(0, 9).c_str(), "RCPT TO:<")) {
      string address = recv_string.substr(9, recv_string.length()-1);
      address = address.substr(0, address.find(">"));

      // TODO how do we check if mbox exists if we can send to users outside system?
      // do we check if the domain is localhost and if so check if row exists?
      if (string_in_vector_smtp(address, &(ms.rcpt_tos))) {
        sprintf(send_buffer, "250 OK address already added\r\n");
      } else {
        ms.rcpt_tos.push_back(address);
        printf("here 204: %ld\n", ms.rcpt_tos.size());
        sprintf(send_buffer, "250 OK\r\n");
      }

      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
    }

    else if (!strcasecmp(recv_string.substr(0, 5).c_str(), "DATA")) {
      printf("in data\n");
      if (ms.mail_from.length() == 0) {
        sprintf(send_buffer, "550 no mail from specified\r\n");
        write_message_smtp(client_fd, send_buffer);
        if (debug_mode) {
          printf("[%d] S: %s", client_fd, send_buffer);
        }
      } else if (ms.rcpt_tos.size() == 0) {
        sprintf(send_buffer, "550 no rcpt to specified\r\n");
        write_message_smtp(client_fd, send_buffer);
        if (debug_mode) {
          printf("[%d] S: %s", client_fd, send_buffer);
        }
      } else {
        ms.recieving_data = true;
        time_t t = time(NULL);
        struct tm time_struct = *localtime(&t);
        sprintf(send_buffer, "From <%s> <%d/%d/%d %d:%d:%d>\r\n", ms.mail_from.c_str(),
                time_struct.tm_mon + 1, time_struct.tm_mday, time_struct.tm_year + 1900,
                time_struct.tm_hour, time_struct.tm_min, time_struct.tm_sec);
        string start_of_message(send_buffer);
        ms.data = start_of_message;

        sprintf(send_buffer, "354 Start mail input; end with <CRLF>.<CRLF>\r\n");
        write_message_smtp(client_fd, send_buffer);
        if (debug_mode) {
          printf("[%d] S: %s", client_fd, send_buffer);
        }
      }
    }

    else if (!strcasecmp(recv_string.substr(0, 5).c_str(), "RSET")) {
      printf("in rset\n");
      bzero(&ms, sizeof(struct mail_state_smtp));
      sprintf(send_buffer, "250 OK\r\n");
      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
    }

    else if (!strcasecmp(recv_string.substr(0, 5).c_str(), "NOOP")) {
      printf("in noop\n");
      sprintf(send_buffer, "250 NOOP\r\n");
      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
    }

    else {
      sprintf(send_buffer, "250 bad command\r\n");
      write_message_smtp(client_fd, send_buffer);
      if (debug_mode) {
        printf("[%d] S: %s", client_fd, send_buffer);
      }
    }

  }

  return NULL;
}


void* run_smtp_server(void* port_number_arg) {
	// set up socket
	int port_number = (long) port_number_arg;
	int socket_fd, client_fd;
	struct sockaddr_in address;
	int addrsize = sizeof(address);
	address.sin_family = AF_INET;
	address.sin_port = htons(port_number);
	address.sin_addr.s_addr = INADDR_ANY;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	int one = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)); // make socket reusable

	int bind_result = bind(socket_fd, (struct sockaddr*) &address, sizeof(address));
	if (bind_result < 0) {
		perror("bind");
		exit(-1);
	}

	int listen_result = listen(socket_fd, MAX_NUM_CONNECTIONS_SMTP_SERVER);
	if (listen_result < 0) {
		perror("listen");
		exit(-1);
	}

	if (debug_mode) {
		printf("Listening on port %d\n", port_number);
	}

	// set up threading
	vector<pthread_t*> threads_vector;
	pthread_t* thread;

	while (keep_running) {
		bzero(&address, sizeof(address));
		client_fd = accept(socket_fd, (struct sockaddr*) &address, (socklen_t*) &addrsize);
		if (client_fd < 0 && errno == 4) {
			break; // interuption
		} else if (client_fd < 0) {
			perror("accept");
			exit(-1);
		}

		thread = (pthread_t*) malloc(sizeof(pthread_t*));
		threads_vector.push_back(thread);
		pthread_create(thread, NULL, smtp_thread_handle_client, (void*) (long) client_fd);
	}

	// clear memory
	for (unsigned long i = 0; i < threads_vector.size(); i++) {
		pthread_kill(*(threads_vector.at(i)), SIGINT);
		pthread_join(*(threads_vector.at(i)), NULL);
		free(threads_vector.at(i));
	}
	threads_vector.clear();
	return NULL;
}
