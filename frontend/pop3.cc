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


volatile bool keep_running = true;
bool debug_mode = false;

int main(int argc, char* argv[]) {
  // Clear signal mask so blocking system calls don't restart when interrupted
  struct sigaction sa = {0};
  sa.sa_handler = sig_handler_pop3;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);

  // argument parsing
  int port_number;
  parse_args_pop3(argc, argv, &port_number);

  run_pop3_server((void*) (long) port_number);

  return 0;
}
