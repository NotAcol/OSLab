#include <array>
#include <bits/types/siginfo_t.h>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

constexpr char kChildCodePath[]{"/home/acol/Documents/OSLab/Askisi-2/child"};

struct ChildData {
  pid_t child_pid{};
  bool gate;
};

void SigAction(int signal, siginfo_t *info, void *);
volatile sig_atomic_t tell_all{}, should_close{};

std::vector<ChildData> dead_kids;

int main(int argc, char *argv[]) {

  if (argc != 2) {
    std::cerr << "Usage : ./a.out [gates]\n";
    return 1;
  }

  size_t child_count{};
  for (; argv[1][child_count] != '\0'; ++child_count) {
    if (argv[1][child_count] != 't' && argv[1][child_count] != 'f') {
      std::cerr << "[gates] must be either 't' or 'f'\n";
      return 1;
    }
  }

  if (child_count == 0) {
    std::cerr << "Usage : ./a.out [gates]\n";
    return 1;
  }

  pid_t my_pid{getpid()};

  struct sigaction act {};
  act.sa_sigaction = SigAction;
  act.sa_flags = SA_RESTART | SA_SIGINFO | SA_NOCLDWAIT;
  if (sigemptyset(&act.sa_mask))
    return 1;
  if (sigaction(SIGUSR1, &act, NULL))
    return 1;

  if (sigaddset(&act.sa_mask, SIGUSR1))
    return 1;
  if (sigaction(SIGCHLD, &act, NULL))
    return 1;

  if (sigaddset(&act.sa_mask, SIGCHLD))
    return 1;
  if (sigaction(SIGTERM, &act, NULL))
    return 1;

  auto children = std::make_unique<pid_t[]>(child_count);
  dead_kids.reserve(child_count);

  for (int i{}; i < child_count; ++i) {
    children[i] = fork();
    if (children[i] < 0) {
      std::cerr << "failed to create child with ID : " << i << " aborting...";
      raise(SIGTERM);
    } else if (children[i] != 0) {
      std::cout << "\033[34m[PARENT/PID=" << my_pid << "] Created child " << i
                << "(PID=" << children[i] << ") and initial state '"
                << argv[1][i] << "'\033[0m\n";
    } else {
      execl(kChildCodePath, kChildCodePath, std::to_string(i).c_str(),
            (argv[1][i] == 't' ? "t" : "f"), NULL);
      std::cerr << "Child: " << getpid() << " failed to execute "
                << kChildCodePath;
      exit(2);
    }
  }

  while (!should_close) {

    while (!dead_kids.empty()) {
      for (size_t i{}; i < child_count; ++i) {
        if (children[i] == dead_kids.back().child_pid) {
          children[i] = fork();
          if (children[i] < 0) {
            std::cerr << "failed to recreate child with ID : " << i
                      << " aborting...";
            raise(SIGTERM);
          } else if (children[i] != 0) {
            std::cout << "\033[34m[PARENT/PID=" << my_pid << "] Recreated child " << i
                      << "(PID=" << children[i] << ") and initial state '"
                      << (dead_kids.back().gate ? "t" : "f") << "'\033[0m\n";
            dead_kids.pop_back();
          } else {
            execl(kChildCodePath, kChildCodePath, std::to_string(i).c_str(),
                  (dead_kids.back().gate ? "t" : "f"), NULL);
            std::cerr << "Child: " << getpid() << " failed to execute "
                      << kChildCodePath;
            exit(2);
          }
        }
      }
    }

    if (tell_all) {
      for (size_t i{}; i < child_count; ++i) {
        kill(children[i], SIGUSR1);
        usleep(1);
      }
      tell_all = 0;
    }

    usleep(50);
  }

  act.sa_handler = SIG_IGN;
  while (sigemptyset(&act.sa_mask)) {
  }
  while (sigaction(SIGCHLD, &act, NULL)) {
  }
  while (sigaction(SIGUSR1, &act, NULL)) {
  }

  for (size_t i{}; i < child_count; ++i) {
    kill(children[i], SIGTERM);
    usleep(1);
  }

  act.sa_handler = SIG_DFL;
  while (sigaction(SIGTERM, &act, NULL)) {
  }
  raise(SIGTERM);
}

void SigAction(int signal, siginfo_t *info, void *) {
  switch (signal) {
  case SIGUSR1:
    tell_all = 1;
    break;
  case SIGCHLD:
    if (info->si_code == CLD_STOPPED) {
      std::cout << "\033[34mresuming child: " << info->si_pid << "\033[0m\n";
      if (kill(info->si_pid, SIGCONT))
        raise(SIGTERM);
    } else if (info->si_code == CLD_CONTINUED) {
      std::cout << "\033[34mchild: " << info->si_pid << " resumed\033[0m" << std::endl;
    } else if (info->si_code == CLD_EXITED) {
      ChildData temp;
      temp.gate = info->si_status == 1;
      temp.child_pid = info->si_pid;
      dead_kids.push_back(temp);
    } else {
      raise(SIGTERM);
    }
    break;
  case SIGTERM:
    should_close = 1;
    break;
  }
}

/*

void SigchldHandler(int signal) {
  int status{};
  pid_t child_pid{waitpid(-1, &status, WNOHANG | WCONTINUED)};

  if (child_pid == -1)
    raise(SIGTERM);
  else if (child_pid == 0)
    return;

  if (WIFEXITED(status)) {
    if (WEXITSTATUS(status) == 2)
      raise(SIGTERM);

    ChildData temp{};
    temp.child_pid = child_pid;
    temp.gate = WEXITSTATUS(status);
    dead_kids.push_back(temp);
    return;
  } else if (WIFSTOPPED(status)) {
    kill(child_pid, SIGCONT);
  } else if (WIFCONTINUED(status)) {
    std::cout << "continued\n";
  } else if (WIFSIGNALED(status)) {
      raise(SIGTERM);
  }
}
*/
