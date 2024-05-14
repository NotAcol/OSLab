#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./a.out filename";
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "--help") == 0) {
        std::cout << "Usage: ./a.out filename";
        return EXIT_SUCCESS;
    }

    struct stat buf;
    if (stat(argv[1], &buf) == 0) {
        std::cerr << "Error: " << argv[1] << " already exists";
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_CREAT | O_APPEND | O_WRONLY);
    if (fd == -1) {
        std::cerr << "Failed to open";
        return EXIT_FAILURE;
    }
    stat(argv[1], &buf);

    int status;
    pid_t child = fork();

    if (child < 0) {
        std::cerr << "Failed to create child";
        return EXIT_FAILURE;
    } else if (child == 0) {
        pid_t Pid{getpid()};
        pid_t Ppid{getppid()};
        std::string output;

        output += "[CHILD] getpid()= " + std::to_string(Pid) +
                  ", getppid()= " + std::to_string(Ppid) + '\n';

        write(fd, output.c_str(), output.size());

        exit(0);
    } else {
        pid_t Pid{getpid()};
        pid_t Ppid{getppid()};
        std::string output;

        output += "[PARENT] getpid()= " + std::to_string(Pid) +
                  ", getppid()= " + std::to_string(Ppid) + '\n';

        wait(&status);
        write(fd, output.c_str(), output.size());
        close(fd);
        exit(0);
    }

    return EXIT_SUCCESS;
}
