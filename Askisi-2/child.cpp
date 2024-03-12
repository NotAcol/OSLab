#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

volatile sig_atomic_t should_close{}, tell_time{1}, timer{}, flip_gate{};
const bool *const p_gate{};

void TellTime(int id, bool gate, pid_t my_pid);
void SigHandler(int signal);

int main(int argc, char *argv[]) {
  pid_t my_pid{getpid()};
  pid_t parent_pid{getppid()};

  bool gate{};
  int id{atoi(argv[1])};
  if (argv[2][id] == 't') {
    gate = true;
  }

  struct sigaction act {};
  act.sa_handler = SigHandler;
  act.sa_flags = SA_RESTART;
  sigemptyset(&act.sa_mask);
  if (sigaction(SIGALRM, &act, NULL)) {
    std::cerr << "failed to set signal action" << std::endl;
    exit(3);
  }
  if (sigaction(SIGUSR1, &act, NULL)) {
    std::cerr << "failed to set signal action" << std::endl;
    exit(3);
  }
  if (sigaction(SIGUSR2, &act, NULL)) {
    std::cerr << "failed to set signal action" << std::endl;
    exit(3);
  }
  sigaddset(&act.sa_mask, SIGALRM);
  sigaddset(&act.sa_mask, SIGUSR1);
  sigaddset(&act.sa_mask, SIGUSR2);
  if (sigaction(SIGTERM, &act, NULL)) {
    std::cerr << "failed to set signal action" << std::endl;
    exit(3);
  }

  alarm(1);

  while (true) {
    if (tell_time == 1) {
      TellTime(id, gate, my_pid);
      tell_time = 0;
    }
    if (flip_gate == 1) {
      gate = !gate;
      flip_gate = 0;
    }
    usleep( 30);
  }

  return 0;
}

void TellTime(int id, bool gate, pid_t my_pid) {
  std::cout << "[ID=" << id << "/PID=" << my_pid << "/TIME=" << timer
            << "s] The gates are " << (gate ? "open" : "closed") << std::endl;
  tell_time = 0;
}

void SigHandler(int signal) {
  switch (signal) {
  case SIGALRM:
    alarm(1);
    ++timer;
    if (timer % 15 == 0)
      tell_time = 1;
    break;
  case SIGUSR1:
    tell_time = 1;
    break;
  case SIGUSR2:
    flip_gate = 1;
    break;
  case SIGTERM:
    *p_gate ? exit(1) : exit(0);
  }
}
