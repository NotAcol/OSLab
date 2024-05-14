#include <cctype>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <poll.h>
#include <sched.h>
#include <string>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>

constexpr int tell_exit{-1};
constexpr size_t int_size = sizeof(tell_exit);

struct ChildInfo {
  pid_t pid;
  int ptoc[2];
  int ctop[2];
};

void KillAll(std::unique_ptr<ChildInfo[]> &children_infos, int child_count);
void ChildCode(int ptoc, int ctop, int ID);

int main(int argc, char *argv[]) {

  //---------------------------INPUT VALIDATION-------------------------------

  std::string exit_messsage(
      "\033[31mUsage: ask3 <nChilder> [--random] [--round-robin]\033[0m\n");

  if (argc < 2 || argc > 3) {
    std::cerr << exit_messsage;
    return 1;
  }
  if (!isdigit(argv[1][0])) {
    std::cerr << exit_messsage;
    return 1;
  }

  int child_count{atoi(argv[1])};

  if (child_count == 0) {
    std::cerr << exit_messsage;
    return 1;
  }

  bool random{};
  if (argc == 3) {
    if (strcmp(argv[2], "--random") == 0)
      random = true;
    else if (strcmp(argv[2], "--round-robin") != 0) {
      std::cerr << exit_messsage;
      return 1;
    }
  }

  //-------------------------SPAWNING CHILDREN--------------------------------

  auto children_infos = std::make_unique<ChildInfo[]>(child_count);

  for (size_t i{}; i < child_count; ++i) {
    if (pipe(children_infos[i].ptoc) || pipe(children_infos[i].ctop)) {
      KillAll(children_infos, child_count);
      std::cerr << "failed opening child to parent pipe";
      return 1;
    }
    children_infos[i].pid = fork();
    if (children_infos[i].pid == -1) {
      KillAll(children_infos, child_count);
      std::cerr << "failed initializing children processes";
      return 1;
    } else if (children_infos[i].pid == 0) {
      close(children_infos[i].ptoc[1]);
      close(children_infos[i].ctop[0]);
      ChildCode(children_infos[i].ptoc[0], children_infos[i].ctop[1], i);
      return 0;
    } else {
      close(children_infos[i].ptoc[0]);
      close(children_infos[i].ctop[1]);
    }
  }

  //-------------------------EVENT LOOP---------------------------------------

  auto pollfds = std::make_unique<pollfd[]>(child_count + 1);
  for (size_t i{}; i < child_count; ++i) {
    pollfds[i].fd = children_infos[i].ctop[0];
    pollfds[i].events |= POLLIN;
  }
  pollfds[child_count].fd = 0;
  pollfds[child_count].events |= POLLIN;

  std::string terminal_input;
  int number_buffer;
  int things_to_read{};
  bool should_close{};

  while (!should_close) {
    things_to_read = poll(pollfds.get(), child_count + 1, -1);

    if ((things_to_read < 0)) {
      std::cerr << "poll failed";
      KillAll(children_infos, child_count);
      return 1;
    }

    if (pollfds[child_count].revents & POLLIN) { // user input from terminal
      --things_to_read;
      std::cin >> terminal_input;
      if (terminal_input == "exit") {
        std::cout << "\033[31mTerminating\033[0m\n";
        should_close = true;
        things_to_read = 0;
      } else if (isdigit(terminal_input.c_str()[0]) &&
                 (number_buffer = atoi(terminal_input.c_str())) != 0) {
        if (random) {
          int random_child{rand() % child_count};
          std::cout << "\033[32m[PARENT] Writing " << number_buffer
                    << " to child " << random_child << "\033[0m" << std::endl;
          write(children_infos[random_child].ptoc[1], &number_buffer, int_size);
        } else {
          std::cout << "\033[32m[PARENT] Writing " << number_buffer
                    << " to child 0\033[0m" << std::endl;
          write(children_infos[0].ptoc[1], &number_buffer, int_size);
        }
      } else {
        std::cout << "\033[33m[PARENT] Type a positive intiger to send job to "
                     "child\033[0m\n";
      }
    }

    for (size_t i{}; (i < child_count) && (things_to_read > 0); ++i) {
      if (pollfds[i].revents & POLLIN) {
        --things_to_read;
        read(children_infos[i].ctop[0], &number_buffer, int_size);
        if (number_buffer > 0) {
          if (random) {
            int random_child{rand() % child_count};
            write(children_infos[random_child].ptoc[1], &number_buffer,
                  int_size);
          } else {
            write(children_infos[(i + 1) % child_count].ptoc[1], &number_buffer,
                  int_size);
          }
        } else if (number_buffer < 0) {
          should_close = true;
        }
      }
    }
  }
  //------------------------------------CLEANUP------------------------------------

  for (size_t i{}; i < child_count; ++i) {
    write(children_infos[i].ptoc[1], &tell_exit, int_size);
    close(children_infos[i].ptoc[1]);
    close(children_infos[i].ctop[0]);
  }
  // KillAll(children_infos, child_count);

  return 0;
}

void ChildCode(int ptoc, int ctop, int ID) {
  int number;
  pid_t my_pid = getpid();
  bool should_close{};
  std::cout << "\033[34m[Child: " << ID << "] Pid: " << my_pid
            << " spawned\033[0m" << std::endl;

  while (!should_close) {
    usleep(50);
    if (read(ptoc, &number, int_size) == -1) {
      write(ctop, &tell_exit, int_size);
    } else if (number == -1) {
      close(ctop);
      close(ptoc);
      should_close = true;
    } else {
      std::cout << "\033[34m[Child: " << ID << "] Received number: " << number--
                << "\033[0m\n";
      sleep(10);
      if (write(ctop, &number, int_size) == -1) {
        write(ctop, &tell_exit, int_size);
      } else {
        std::cout << "\033[36m[Child: " << ID
                  << "] Returning number: " << number << "\033[0m\n";
      }
    }
  }
  std::cout << "\033[31m[Child: " << ID << "] Exiting\033[0m" << std::endl;
}

void KillAll(std::unique_ptr<ChildInfo[]> &children_infos, int child_count) {
  for (size_t i{}; i < child_count; ++i) {
    kill(children_infos[i].pid, SIGTERM);
  }
}
