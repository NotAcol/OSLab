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
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

constexpr bool kGateClosed{1}, kGateOpen{0};
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
  size_t i{};
  for (; argv[1][i] != '\0'; ++i) {
    if (argv[1][i] == 't' || argv[1][i] == 'f') {
      ++gate_count;
    } else {
      std::cerr << "[gates] must be either 't' or 'f'";
      exit(1);
    }
  }
  std::cout << "number of gates : " << i;

  pid_t parent_pid = getpid();

  std::vector<int> children_pids(gate_count); // maybe bug if int isnt 32bit
  p_children_pids = &children_pids;
  size_t child_id{};

  struct sigaction act {};
  act.sa_sigaction = SigAction;
  act.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  if (sigaction(SIGUSR1, &act, NULL)) {
    std::cerr << "failed to set sigaction" << std::endl;
    raise(SIGTERM);
  }

  sigaddset(&act.sa_mask, SIGUSR1);
  sigaddset(&act.sa_mask, SIGTERM);
  if (sigaction(SIGCHLD, &act, NULL)) {
    std::cerr << "failed to set sigaction" << std::endl;
    raise(SIGTERM);
  }

  sigaddset(&act.sa_mask, SIGCHLD);
  if (sigaction(SIGTERM, &act, NULL)) {
    std::cerr << "failed to set sigaction" << std::endl;
    raise(SIGTERM);
  }

  for (; child_id < gate_count; ++child_id) {
    children_pids[child_id] = fork();
    if (children_pids[child_id] < 0) {
      std::cerr << "failed to create child, aborting";
      raise(SIGTERM);
    } else if (children_pids[child_id] != 0) {
      std::cout << "[PARENT/PID=" << parent_pid << "] Created child "
                << child_id << " (PID=" << children_pids[child_id]
                << ") and initial state '" << argv[1][child_id] << '\n';
    } else {
      execl(kChildPath, kChildPath, std::to_string(child_id).c_str(), argv[1],
            NULL);
      std::cerr << "failed to execute child code" << std::endl;
      kill(parent_pid, SIGTERM);
      exit(3);
    }
  }

  while (true) {
    if (tell_all) {
      for (const auto &child : children_pids) {
        kill(child, SIGUSR1);
      }
      tell_all = 0;
    }
    usleep(50);
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
    if (info->si_code == CLD_STOPPED) {
      std::cout << "resuming child : " << info->si_pid << '\n';
      if (kill(info->si_pid, SIGCONT))
        raise(SIGTERM);
      // int status;
      // waitpid(info->si_pid, &status, WNOHANG);
      // if (WIFCONTINUED(status)) {
      //   std::cout << "succeeded\n";
      //   break;
      // }
      // raise(SIGTERM);
    } else if (info->si_code == CLD_CONTINUED) {
      std::cout << "child : " << info->si_pid << " resumed" << std::endl;
    } else if (info->si_code == CLD_EXITED) {
      std::cout << "recreating child" << std::endl;
      for (int i{}; i < p_children_pids->size(); ++i) {
        if (p_children_pids->at(i) == info->si_pid) {
          p_children_pids->data()[i] = fork(); //remember to change!
          if (p_children_pids->at(i) < 0) {
            std::cerr << "failed to recreate child, aborting" << std::endl;
            raise(SIGTERM);
          } else if (p_children_pids->at(i) != 0) {
            std::cout << "Recreated child [ID/" << i
                      << "] with new pid : " << p_children_pids->at(i)
                      << std::endl;
          } else {
            execl(kChildPath, kChildPath, std::to_string(i).c_str(),
                  (info->si_status ? "f" : "t"), NULL);
            std::cerr << "failed to execute child code" << std::endl;
            exit(3);
          }
        }
      }
    } else {
      raise(SIGTERM);
    }
    break;

  case SIGTERM:
    for (const auto &pid : *p_children_pids) {
      kill(static_cast<pid_t>(pid), SIGTERM);
    }
    struct sigaction temp {};
    temp.sa_handler = SIG_DFL;
    sigemptyset(&temp.sa_mask);
    sigaddset(&temp.sa_mask, SIGCHLD);
    sigaddset(&temp.sa_mask, SIGUSR1);
    sigaddset(&temp.sa_mask, SIGCHLD);
    sigaction(SIGTERM, &temp, NULL);
    raise(SIGTERM);
  }
}
