#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

volatile int timer{};
volatile sig_atomic_t should_close{}, tell_time{1}, flip{};

void SigHandler(int signal);

int main(int argc, char *argv[]) {
  pid_t my_pid{getpid()};
  pid_t parent_pid{getppid()};

  int ID{atoi(argv[1])};
  bool gate{argv[2][0] == 't' ? true : false};

  struct sigaction act {};
  act.sa_handler = SigHandler;
  act.sa_flags = SA_RESTART;
  if (sigemptyset(&act.sa_mask))
    exit(2);
  if (sigaction(SIGUSR1, &act, NULL) || sigaction(SIGUSR2, &act, NULL) ||
      sigaction(SIGALRM, &act, NULL) || sigaction(SIGTERM, &act, NULL)) {
    exit(2);
  }

  alarm(1);

  while (!should_close) {
    if (flip) {
      gate = !gate;
      flip = 0;
    }
    if (tell_time) {
      std::cout << (gate ? "\033[32m" : "\033[31m") << "[ID=" << ID
                << "/PID=" << my_pid << "/TIME=" << timer << "s] The gates are "
                << (gate ? "open!" : "closed!") << "\033[0m\n";
      tell_time = 0;
    }
    usleep(50);
  }

  std::cerr << "\033[33mexiting child: " << my_pid << "\033[0m"<< std::endl;
  gate ? exit(1) : exit(0);
}

void SigHandler(int signal) {
  switch (signal) {
  case SIGUSR1:
    tell_time = 1;
    break;
  case SIGUSR2:
    flip = 1;
    break;
  case SIGALRM:
    alarm(1);
    if (++timer % 15 == 0)
      tell_time = 1;
    break;
  case SIGTERM:
    should_close = 1;
    break;
  }
}
