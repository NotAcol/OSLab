#include <bits/types/siginfo_t.h>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

void SigAction(int signal, siginfo_t *info, void *);

constexpr char kChildPath[]{"/home/acol/Documents/OSLab/Askisi-2/child"};
volatile sig_atomic_t tell_all{};
std::vector<int> *p_children_pids;

int main(int argc, char *argv[]) {

  if (argc != 2) {
    std::cerr << "Usage: ./a.out [gates]";
    exit(1);
  }
  if (argv[1][0] == '\0') {
    std::cerr << "Usage: ./a.out [gates]";
    exit(1);
  }

  size_t gate_count{};

  for (size_t i{}; argv[1][i] != '\0'; ++i) {
    if (argv[1][i] == 't' || argv[1][i] == 'f') {
      ++gate_count;
    } else {
      std::cerr << "[gates] must be either 't' or 'f'";
      exit(1);
    }
  }

  pid_t parent_pid = getpid();

  std::vector<int> children_pids(gate_count); // maybe bug if int isnt 32bit

  size_t child_id{};

  for (; child_id < gate_count; ++child_id) {
    children_pids[child_id] = fork();
    if (children_pids[child_id] < 0) {
      std::cerr << "failed to create child, aborting";
      exit(1);
    } else if (children_pids[child_id] != 0) {
      std::cout << "[PARENT/PID=" << parent_pid << "] Created child "
                << child_id << " (PID=" << children_pids[child_id]
                << ") and initial state '" << argv[1][child_id] << '\n';
    } else {
      execl(kChildPath, kChildPath,
            std::to_string(child_id).c_str(), // sprintf ?
            argv[1], NULL);
      std::cerr << "failed to execute child code" << std::endl;
      kill(parent_pid, SIGQUIT);
      exit(3);
    }
  }

  struct sigaction act {};
  act.sa_sigaction = SigAction;
  act.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  sigaction(SIGUSR1, &act, NULL);

  sigaddset(&act.sa_mask, SIGUSR1);
  sigaddset(&act.sa_mask, SIGTERM);
  sigaction(SIGCHLD, &act, NULL);

  sigaddset(&act.sa_mask, SIGCHLD);
  sigaction(SIGTERM, &act, NULL);

  while (true) {
    if (tell_all) {
      for (const auto &child : children_pids) {
        kill(child, SIGUSR1);
      }
      tell_all = 0;
    }
    usleep(30);
  }

  return 0;
}

void SigAction(int signal, siginfo_t *info, void *) {
  switch (info->si_signo) {

  case SIGUSR1:
    if (!tell_all)
      tell_all = 1;
    break;

  case SIGCHLD:
    if (info->si_status == SIGSTOP || info->si_status == SIGTSTP) {
        std::cout <<  info->si_pid << std::endl;
      std::cout << "continuing child : " << info->si_pid << std::endl;
      kill(info->si_pid, SIGCONT);
    } else if (info->si_status == 3) {
      raise(SIGKILL);
    } else {
      pid_t temp{};
      char state[2];
      state[0] = info->si_status == 0 ? 'f' : 't';
      state[1] = '\0';
      for (int i{}; i < p_children_pids->size(); ++i) {
        if (p_children_pids->at(i) == info->si_pid) {
          p_children_pids->at(i) = fork();
          if (p_children_pids->at(i) < 0) {
            std::cerr << "failed to recreate child, aborting" << std::endl;
            exit(1);
          } else if (p_children_pids->at(i) != 0) {
            std::cout << "Recreated child  " << i
                      << " with new pid : " << p_children_pids->at(i)
                      << std::endl;
          } else {
            execl(kChildPath, kChildPath,
                  std::to_string(i).c_str(), // sprintf ?
                  state, NULL);
            std::cerr << "failed to execute child code" << std::endl;
            exit(3);
          }
        }
      }
    }
    break;

  case SIGTERM:
    for (const auto &pid : *p_children_pids) {
      kill(pid, SIGTERM);
    }
    struct sigaction temp {};
    temp.sa_handler = SIG_DFL;
    sigaction(SIGTERM, &temp, NULL);
    raise(SIGTERM);
  }
}
