#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <ios>
#include <iostream>
#include <limits>
#include <string>

void ParseGet(char *read_buffer);

int main(int argc, char *argv[]) {
    std::string HOST = "os4.iot.dslab.ds.open-cloud.xyz";
    std::string PORT = "20241";
    bool debug{};

    for (size_t i{1}; i < argc; ++i) {
        if (strcmp(argv[i], "--host") == 0) {
            HOST = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--port") == 0) {
            PORT = argv[i + 1];
            ++i;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug = true;
        } else {
            std::cerr << "invalid arguments\n";
            return 1;
        }
    }

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0) {
        std::cerr << "couldnt find suitable socket\n";
        return 1;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(0);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sd, reinterpret_cast<sockaddr *>(&sin), sizeof(sin))) {
        std::cerr << "failed to bind the socket\n";
        return 1;
    }

    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *server_info;

    if (getaddrinfo(HOST.c_str(), PORT.c_str(), &hints, &server_info)) {
        std::cout << "couldnt get server info\n";
        return 1;
    };

    if (connect(sd, server_info->ai_addr, server_info->ai_addrlen)) {
        std::cout << "failed creating connection to server\n";
        return 1;
    }

    std::string input;
    input.reserve(100);
    char read_buffer[101];
    int read_length{};

    fd_set read_fds;

    // NOTE(Panagiotis): why ever use select over poll
    // NOTE(Panagiotis): why do I even need poll or select, server is never
    // sending without being asked

    for (;;) {
        FD_ZERO(&read_fds);
        FD_SET(sd, &read_fds);
        FD_SET(0, &read_fds);

        std::cout << "\033[0;31m>> \033[0m";
        std::cout.flush();

        if (select(sd + 1, &read_fds, nullptr, nullptr, nullptr) == -1) {
            std::cerr << "select failed exiting\n";
            return 1;
        }
        if (FD_ISSET(0, &read_fds)) {
            std::getline(std::cin, input);

            if (input == "help") {
                std::cout
                    << "some help message\n";  // TODO(Panagiotis): help message
            } else if (input == "get") {
                if (debug) std::cout << "\033[0;33m[DEBUG] sent 'get'\033[0m\n";

                while (write(sd, input.c_str(), input.size() + 1) == -1)
                    usleep(500);

                read_length = read(sd, read_buffer, 100);
                read_buffer[read_length - 1] = '\0';
                if (debug)
                    std::cout << "\033[0;33m[DEBUG] read '" << read_buffer
                              << "'\033[0m\n";
                ParseGet(read_buffer);

            } else if (input == "exit") {
                return 0;

            } else if (isdigit(input[0])) {
                while (write(sd, input.c_str(), input.size() + 1) == -1)
                    usleep(500);
                if (debug)
                    std::cout << "\033[0;33m[DEBUG] sent '" << input
                              << "'\033[0m\n";

                read_length = read(sd, read_buffer, 100);
                read_buffer[read_length - 1] = '\0';
                if (debug)
                    std::cout << "\033[0;33m[DEBUG] read '" << read_buffer
                              << "'\033[0m\n";

                if (strcmp(read_buffer, "try again") == 0) {
                    std::cout << "try again\n";
                    continue;
                }

                std::cout << "\033[0;36mSend verification code: \033[0m'"
                          << read_buffer << "'\n";

                std::cout << "\033[0;31m>> \033[0m";
                std::cout.flush();

                std::cin >> input;

                while (write(sd, input.c_str(), input.size() + 1) == -1)
                    usleep(500);

                if (debug)
                    std::cout << "\033[0;33m[DEBUG] sent '" << input
                              << "'\033[0m\n";

                read_length = read(sd, read_buffer, 100);
                read_buffer[read_length - 1] = '\0';
                if (debug)
                    std::cout << "\033[0;33m[DEBUG] read '" << read_buffer
                              << "'\033[0m\n";

                std::cout << "\033[0;36mResponse: \033[0m'" << read_buffer
                          << "'\n";
            }
        }
        if (FD_ISSET(sd, &read_fds)) {
            read_length = read(sd, read_buffer, 100);
            read_buffer[read_length - 1] = '\0';
            if (debug)
                std::cout
                    << "\033[0;33m[DEBUG] received unexpected message \033[0m'"
                    << read_buffer << "'\n";
            std::cout << read_buffer << '\n';
        }
    }
    return 0;
}

void ParseGet(char *read_buffer) {
    std::cout << "\033[0;36mLatest event: \033[0m";
    switch (read_buffer[0] - '0') {
        case 0: {
            std::cout << "boot(0)\n";
        } break;
        case 1: {
            std::cout << "setup(1)\n";
        } break;
        case 2: {
            std::cout << "interaval(2)\n";
        } break;
        case 3: {
            std::cout << "button(3)\n";

        } break;
        case 4: {
            std::cout << "motion(4)\n";
        } break;
    }

    std::cout << "\033[0;36mLight level: \033[0m";
    int i{2};
    int light_level{};
    for (; read_buffer[i] != ' '; ++i) {
        light_level *= 10;
        light_level += read_buffer[i] - '0';
    }
    std::cout << light_level << '\n';

    std::cout << "\033[0;36mTemperature: \033[0m";
    ++i;
    float temp{};
    for (; read_buffer[i] != ' '; ++i) {
        temp *= 10;
        temp += read_buffer[i] - '0';
    }
    temp /= 100;
    std::cout << temp << '\n';

    ++i;
    time_t timestamp{};
    for (; read_buffer[i] != '\0'; ++i) {
        timestamp *= 10;
        timestamp += read_buffer[i] - '0';
    }
    std::cout << "\033[0;36mTimestamp: \033[0m";
    struct tm *date;
    date = localtime(&timestamp);

    std::cout << date->tm_year + 1900 << '-' << date->tm_mon << '-'
              << date->tm_mday << ' ' << date->tm_hour << ':' << date->tm_min
              << ':' << date->tm_sec << '\n';
}
