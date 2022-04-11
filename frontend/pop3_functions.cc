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


#include "common_mail_files.h"
#include "mail.h"
#include "pop3.h"

using namespace std;

#define MAX_NUM_CONNECTIONS_POP3 100
#define MAX_BUFFER_LENGTH_POP3 100000


extern volatile bool keep_running;
extern bool debug_mode;


//Signal handler for SIGINT so that program can exit gracefully
void sig_handler_pop3(int sig) {
	if (sig == SIGINT) {
		keep_running = false;
	}
}


// Function to parse command-line arguments and populate relevent variables
void parse_args_pop3(int argc, char** argv, int* port_number) {
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


void clear_mail_state_pop3(struct mail_state_pop3* ms) {
  ms->user = "";
  ms->state = 'a';
}


// Function to read a message from a socket (represented by fd) into buffer
int read_message_pop3(int fd, char* buffer) {
	int read_result;
	for (int j = 0; j < MAX_BUFFER_LENGTH_POP3; buffer[j++] = 0);

	for (int i = 0; i < MAX_BUFFER_LENGTH_POP3; i++) {
		read_result = read(fd, buffer + i, 1);
		if (read_result == 0) {
			return 0;
		} else if (read_result == -1 && errno == 4) {
			return 2;
		} else if (i > 1 && buffer[i - 1] == '\r' && buffer[i] == '\n') {
			buffer[i - 1] = 0;
			return 1;
		}
	}
	return -1;
}


// Function to write a message stored in buffer to a socket (represented by fd)
int write_message_pop3(int fd, char* buffer) {
	return send(fd, buffer, strlen(buffer), 0);
}


// function to write a multi-line string to the client
void multi_line_write_pop3(int fd, const char* string) {
	char* string_copy = (char*) malloc((strlen(string) + 1) * sizeof(char));
	char* current;
	bzero(string_copy, (strlen(string) + 1) * sizeof(char));
	strcpy(string_copy, string);

	char* token = strtok(string_copy, "\n");
	while (token != NULL) {
		current = (char*) malloc((strlen(token) + 3) * sizeof(char));
		bzero(current, (strlen(token) + 3) * sizeof(char));
		sprintf(current, "%s\r\n", token);
		write_message_pop3(fd, current);
		if (debug_mode) printf("[%d] S: %s\r\n", fd, token);
		token = strtok(NULL, "\n");
		free(current);
	}
	free(string_copy);
}



int handle_message_pop3(char* message, int fd, struct mail_state_pop3* ms, vector<string>* UIDLs_to_delete) {
  int return_value = 0;
  char buffer[MAX_BUFFER_LENGTH_POP3];
  string message_string = string(message);
  auto tokens = separate_string(message_string, " ");
  unsigned long num_tokens = tokens.size();

	KeyValueClient client;


  if (num_tokens > 1 && !strcasecmp(tokens[0].c_str(), "USER")) {
    string username(message + 5);
    if (ms->state != 'a') {
      sprintf(buffer, "-ERR not in authentication state\r\n");
    }

		// else if (user_exists(username, client)) {
    //   ms->user = username;
    //   sprintf(buffer, "+OK %s is a valid user\r\n", message + 5);
    // }

		else {
      ms->user = username;
      sprintf(buffer, "+OK %s is a valid user\r\n", message + 5);
    }

		//  else {
    //   clear_mail_state_pop3(ms);
    //   sprintf(buffer, "-ERR %s is not a valid user\r\n", message + 5);
    // }

    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
  }

  else if (num_tokens > 1 && !strcasecmp(tokens[0].c_str(), "PASS")) {
    if (ms->state != 'a') {
      sprintf(buffer, "-ERR not in authentication state\r\n");
    } else if (ms->user == "") {
      sprintf(buffer, "-ERR no user specified\r\n");
    } else {

			MasterBackendClient master_client(
				grpc::CreateChannel(
					"localhost:5000",
					grpc::InsecureChannelCredentials()));

			string channel = master_client.which_node_call(ms->user);
			client = KeyValueClient(
				grpc::CreateChannel(
					channel,
					grpc::InsecureChannelCredentials()));

      string correct_password = get_user_password(ms->user, client);

			cout << "CORRECT PASSWORD: " << correct_password << endl;

      string input_password(message + 5);
      if (input_password == correct_password) {
        ms->state = 't';
        sprintf(buffer, "+OK authenticated. Now in transaction mode\r\n");
      } else {
        sprintf(buffer, "-ERR incorrect password\r\n");
      }
    }
    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
  }

  else if (num_tokens == 1 && !strcasecmp(tokens[0].c_str(), "STAT")) {
    // +OK num_messages size
    if (ms->state != 't') {
      sprintf(buffer, "-ERR must be in transaction state\r\n");
    } else {
			MasterBackendClient master_client(
				grpc::CreateChannel(
					"localhost:5000",
					grpc::InsecureChannelCredentials()));

			string channel = master_client.which_node_call(ms->user);
			client = KeyValueClient(
				grpc::CreateChannel(
					channel,
					grpc::InsecureChannelCredentials()));

      int num_messages = get_num_messages(ms->user, client);
      int maildrop_size = get_maildrop_size(ms->user, client);
      sprintf(buffer, "+OK %d %d\r\n", num_messages, maildrop_size);
    }
    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
  }

  else if ((num_tokens == 1 || num_tokens == 2) && !strcasecmp(tokens[0].c_str(), "UIDL")) {
    if (ms->state != 't') {
      sprintf(buffer, "-ERR must be in transaction state\r\n");
      write_message_pop3(fd, buffer);
      if (debug_mode) printf("[%d] S: %s", fd, buffer);
    } else {
      // messages = get_messages(directory_name, ms->user, &num_messages, &maildrop_size, &message_lengths, &header_lengths);

			MasterBackendClient master_client(
				grpc::CreateChannel(
					"localhost:5000",
					grpc::InsecureChannelCredentials()));

			string channel = master_client.which_node_call(ms->user);
			client = KeyValueClient(
				grpc::CreateChannel(
					channel,
					grpc::InsecureChannelCredentials()));

      string email_metadata = list_email_hashes(ms->user, client);
      auto email_hashes = separate_string(email_metadata, ";");
      int num_messages = email_hashes.size();

      if (num_tokens == 1) {
        sprintf(buffer, "+OK\r\n");
        write_message_pop3(fd, buffer);
        if (debug_mode) printf("[%d] S: %s", fd, buffer);
        for (unsigned long m = 0; m < email_hashes.size(); m++) {
          string current_UIDL = email_hashes[m];
          sprintf(buffer, "%ld %s\r\n", m + 1, current_UIDL.c_str());
          write_message_pop3(fd, buffer);
          if (debug_mode) printf("[%d] S: %s", fd, buffer);
        }
        sprintf(buffer, ".\r\n");
        write_message_pop3(fd, buffer);
        if (debug_mode) printf("[%d] S: %s", fd, buffer);
      }

      else {
        int message_num = atoi(tokens[1].c_str());
        if (message_num > num_messages || message_num < 1) {
          sprintf(buffer, "-ERR no such message\r\n");
          write_message_pop3(fd, buffer);
          if (debug_mode) printf("[%d] S: %s", fd, buffer);
        } else {
          string current_UIDL = email_hashes[message_num - 1];
          sprintf(buffer, "+OK %d %s\r\n", message_num, current_UIDL.c_str());
          write_message_pop3(fd, buffer);
          if (debug_mode) printf("[%d] S: %s", fd, buffer);
        }
      }
    }
  }

  else if (num_tokens == 2 && !strcasecmp(tokens[0].c_str(), "RETR")) {
    if (ms->state != 't') {
      sprintf(buffer, "-ERR must be in transaction state\r\n");
      write_message_pop3(fd, buffer);
      if (debug_mode) printf("[%d] S: %s", fd, buffer);
    } else {

			MasterBackendClient master_client(
				grpc::CreateChannel(
					"localhost:5000",
					grpc::InsecureChannelCredentials()));

			string channel = master_client.which_node_call(ms->user);
			client = KeyValueClient(
				grpc::CreateChannel(
					channel,
					grpc::InsecureChannelCredentials()));

      string email_metadata = list_email_hashes(ms->user, client);
      auto email_hashes = separate_string(email_metadata, ";");
      int num_messages = email_hashes.size();
      int message_num = atoi(tokens[1].c_str());

      if (message_num > num_messages || message_num < 1) {
        sprintf(buffer, "-ERR no such message\r\n");
        write_message_pop3(fd, buffer);
        if (debug_mode) printf("[%d] S: %s", fd, buffer);
      } else {

				MasterBackendClient master_client(
					grpc::CreateChannel(
						"localhost:5000",
						grpc::InsecureChannelCredentials()));

				string channel = master_client.which_node_call(ms->user);
				client = KeyValueClient(
					grpc::CreateChannel(
						channel,
						grpc::InsecureChannelCredentials()));

        string email_data = get_email_from_hash(ms->user, email_hashes[message_num - 1], client);
        int email_length = email_data.length();
        sprintf(buffer, "+OK %d octets\r\n", email_length);
        write_message_pop3(fd, buffer);
        if (debug_mode) printf("[%d] S: %s", fd, buffer);
        multi_line_write_pop3(fd, email_data.c_str());
        sprintf(buffer, ".\r\n");
        write_message_pop3(fd, buffer);
        if (debug_mode) printf("[%d] S: %s", fd, buffer);
      }
    }
  }

  else if (num_tokens == 2 && !strcasecmp(tokens[0].c_str(), "DELE")) {
    if (ms->state != 't') {
			sprintf(buffer, "-ERR must be in transaction state\r\n");
		} else {

			MasterBackendClient master_client(
				grpc::CreateChannel(
					"localhost:5000",
					grpc::InsecureChannelCredentials()));

			string channel = master_client.which_node_call(ms->user);
			client = KeyValueClient(
				grpc::CreateChannel(
					channel,
					grpc::InsecureChannelCredentials()));

      string email_metadata = list_email_hashes(ms->user, client);
      auto email_hashes = separate_string(email_metadata, ";");
      int num_messages = email_hashes.size();
			int message_num = atoi(tokens[1].c_str());

			if (message_num > num_messages || message_num < 1) {
				sprintf(buffer, "-ERR no such message\r\n");
			} else {
				string current_UIDL = email_hashes[message_num - 1];
				if (string_in_vector(current_UIDL, UIDLs_to_delete)) {
					sprintf(buffer, "-ERR message already marked for delete\r\n");
				} else {
					UIDLs_to_delete->push_back(current_UIDL);
					sprintf(buffer, "+OK\r\n");
				}
			}
		}
    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
  }

  else if ((num_tokens == 1 || num_tokens == 2) && !strcasecmp(tokens[0].c_str(), "LIST")) {
    if (ms->state != 't') {
      sprintf(buffer, "-ERR must be in transaction state\r\n");
      write_message_pop3(fd, buffer);
      if (debug_mode) printf("[%d] S: %s", fd, buffer);
    }

    else {

			MasterBackendClient master_client(
				grpc::CreateChannel(
					"localhost:5000",
					grpc::InsecureChannelCredentials()));

			string channel = master_client.which_node_call(ms->user);
			client = KeyValueClient(
				grpc::CreateChannel(
					channel,
					grpc::InsecureChannelCredentials()));

      string email_metadata = list_email_hashes(ms->user, client);
      auto email_hashes = separate_string(email_metadata, ";");
      int num_messages = email_hashes.size();
      int maildrop_size = get_maildrop_size(ms->user, client);

      if (num_tokens == 1) {
        sprintf(buffer, "+OK %d messages (%d octets)\r\n", num_messages, maildrop_size);
        write_message_pop3(fd, buffer);
        if (debug_mode) printf("[%d] S: %s", fd, buffer);
        for (int m = 0; m < num_messages; m++) {
          string email_data = get_email_from_hash(ms->user, email_hashes[m], client);
          int email_size = email_data.length();
          sprintf(buffer, "%d %d\r\n", m + 1, email_size);
          write_message_pop3(fd, buffer);
          if (debug_mode) printf("[%d] S: %s", fd, buffer);
        }
        sprintf(buffer, ".\r\n");
        write_message_pop3(fd, buffer);
        if (debug_mode) printf("[%d] S: %s", fd, buffer);
      }

      else if (num_tokens == 2) {
        int message_num = atoi(tokens[1].c_str());
        if (message_num > num_messages || message_num < 1) {
          sprintf(buffer, "-ERR no such message %d %d %s\r\n", message_num, num_messages, tokens[1].c_str());
          write_message_pop3(fd, buffer);
          if (debug_mode) printf("[%d] S: %s", fd, buffer);
        } else {
          string email_data = get_email_from_hash(ms->user, email_hashes[message_num - 1], client);
          int email_size = email_data.length();
          sprintf(buffer, "+OK %d %d\r\n", message_num, email_size);
          write_message_pop3(fd, buffer);
          if (debug_mode) printf("[%d] S: %s", fd, buffer);
        }
      }
    }
  }

  else if (num_tokens == 1 && !strcasecmp(tokens[0].c_str(), "RSET")) {
    if (ms->state != 't') {
      sprintf(buffer, "-ERR must be in transaction state\r\n");
    } else {
      UIDLs_to_delete->clear();
      sprintf(buffer, "+OK\r\n");
    }
    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
  }

  else if (num_tokens == 1 && !strcasecmp(tokens[0].c_str(), "QUIT")) {
    if (ms->state == 't') {
      for (unsigned long m = 0; m < UIDLs_to_delete->size(); m++) {
        string current_UIDL = UIDLs_to_delete->at(m);
        delete_email_from_hash(ms->user, current_UIDL, client);
      }
    }

    sprintf(buffer, "+OK Goodbye!\r\n");
    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
    return_value = 1;
  }

  else if (num_tokens == 1 && !strcasecmp(tokens[0].c_str(), "NOOP")) {
    sprintf(buffer, "+OK\r\n");
    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
  }

  else {
    sprintf(buffer, "-ERR Not supported\r\n");
    write_message_pop3(fd, buffer);
    if (debug_mode) {
      printf("[%d] S: %s", fd, buffer);
    }
  }

  return return_value;
}


void* pop3_thread_handle_client(void* argument_pointer) {

  int client_fd = (long) argument_pointer;
  char read_buffer[MAX_BUFFER_LENGTH_POP3];
  int read_value, handle_val;

  vector<string> UIDLs_to_delete;
  struct mail_state_pop3 ms;
  clear_mail_state_pop3(&ms);

  if (debug_mode) {
    printf("[%d] New connection\n", client_fd);
  }

  sprintf(read_buffer, "+OK Server ready\r\n");
  write_message_pop3(client_fd, read_buffer);
  if (debug_mode) {
    printf("[%d] S: %s", client_fd, read_buffer);
  }

  while (true) {
    read_value = read_message_pop3(client_fd, read_buffer);

    if (read_value == 1 && debug_mode) {
      printf("[%d] C: %s\n", client_fd, read_buffer);
    }

    if (read_value == 0) {
      if (debug_mode) {
        printf("[%d] Connection closed\n", client_fd);
      }
      close(client_fd);
      UIDLs_to_delete.clear();
      return 0;
    }

    if (!keep_running) {
      sprintf(read_buffer, "-ERR Server shutting down\r\n");
      write_message_pop3(client_fd, read_buffer);
      if (debug_mode) {
        printf("[%d] C: %s", client_fd, read_buffer);
      }
      close(client_fd);
      UIDLs_to_delete.clear();
      return 0;
    }

    handle_val = handle_message_pop3(read_buffer, client_fd, &ms, &UIDLs_to_delete);

    if (handle_val == 1) {
      close(client_fd);
      UIDLs_to_delete.clear();
      return 0;
    }

  }

  return NULL;
}


void* run_pop3_server(void* port_number_arg) {
  int portno = (long) port_number_arg;

  int socket_fd, client_fd;
	struct sockaddr_in address;
	int addrsize = sizeof(address);

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);

	address.sin_family = AF_INET;
	address.sin_port = htons(portno);
	address.sin_addr.s_addr = INADDR_ANY;

	// make socket reusable
	int one = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  if (bind(socket_fd, (struct sockaddr*) &address, sizeof(address)) < 0) {
		perror("bind");
		exit(-1);
	}

	if (listen(socket_fd, MAX_NUM_CONNECTIONS_POP3) < 0) {
		perror("listen");
		exit(-1);
	}

  if (debug_mode) {
    printf("Listening on port %d\n", portno);
  }

  vector<pthread_t*> threads_vector;
  pthread_t* thread;

  while (keep_running) {

    bzero(&address, sizeof(address));

    if ((client_fd = accept(socket_fd, (struct sockaddr*) &address, (socklen_t*) &addrsize)) < 0) {
      if (errno == 4) {
        break; // interuption
      } else {
        continue; // other error (just ignore)
      }
    }

    thread = (pthread_t*) malloc(sizeof(pthread_t*));
    threads_vector.push_back(thread);
    pthread_create(thread, NULL, pop3_thread_handle_client, (void*) (long) client_fd);

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
