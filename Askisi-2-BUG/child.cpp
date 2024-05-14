#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

constexpr int kGateClosed{1}, kGateOpen{0};
volatile bool should_close{}, tell_time{true}, flip_gate{};
volatile int timer{};

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

  while (!should_close) {
    if (tell_time) {
      TellTime(id, gate, my_pid);
      tell_time = false;
    }
    if (flip_gate) {
      gate = !gate;
      flip_gate = false;
    }
    usleep(50);
  }
  std::cerr << "exiting child : " << my_pid << std::endl;
  gate ? exit(kGateOpen) : exit(kGateClosed);
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
      tell_time = true;
    break;
  case SIGUSR1:
    tell_time = true;
    break;
  case SIGUSR2:
    flip_gate = true;
    break;
  case SIGTERM:
    should_close = true;
    break;
  }
}
