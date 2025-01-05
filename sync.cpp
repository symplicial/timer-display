#include "sync.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <stdio.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <thread>

std::mutex timerValueMutex;
int64_t timerValue;

// LiveSplit sends times using the "constant" ("c") standard TimeSpan format:
// https://learn.microsoft.com/en-us/dotnet/standard/base-types/standard-timespan-format-strings#the-constant-c-format-specifier 
// [-][d.]hh:mm:ss[.fffffff]
// Returns a number in milliseconds
int64_t parseTimespan(std::string str) {
    std::string timespan = str;
    int sign = 1;
    if (str[0] == '-') {
        sign = -1;
        timespan = str.substr(1);
    }

    size_t dotPos = timespan.find_first_of(".");
    size_t colonPos = timespan.find_first_of(":");

    // Handle case in which neither occurs and the string is invalid
    int days = 0;
    if (dotPos < colonPos) {
        // We have days
        days = std::stoi(timespan.substr(0, dotPos));
        timespan = timespan.substr(dotPos + 1);
    }

    colonPos = timespan.find_first_of(":");
    int hrs = std::stoi(timespan.substr(0, colonPos));
    timespan = timespan.substr(colonPos + 1);

    colonPos = timespan.find_first_of(":");
    int mins = std::stoi(timespan.substr(0, colonPos));
    timespan = timespan.substr(colonPos + 1);

    dotPos = timespan.find_first_of(".");
    int seconds = 0;
    int ms = 0;
    if (dotPos == std::string::npos) {
        // Only seconds
        seconds = std::stoi(timespan);
    } else {
        seconds = std::stoi(timespan.substr(0, dotPos));
        ms = std::stoi(timespan.substr(dotPos + 1, 3));
    }

    return sign * (ms + (seconds * 1000) + (mins * 1000 * 60) + (hrs * 1000 * 60 * 60) + (days * 1000 * 60 * 60 * 24));
}


void syncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        const auto t1 = std::chrono::system_clock::now();
        const char *msg = "getcurrenttime\r\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        const auto t2 = std::chrono::system_clock::now();

        buf[n] = '\0';
        {
            std::lock_guard<std::mutex> guard(timerValueMutex);
            timerValue = parseTimespan(std::string(buf));
        }
        //printf("got message: %s\n", buf);
        //printf("time: %lld\n", parseTimespan(std::string(buf)));

        auto diff = t2 - t1;
        //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(diff).count() << "\n";

        auto t1c = std::chrono::system_clock::to_time_t(t1);
        //std::cout << std::put_time(std::localtime(&t1c), "%c") << "\n";

        auto t = std::chrono::system_clock::now();
        t += std::chrono::milliseconds(100);
        std::this_thread::sleep_until(t);
    }

    close(sock);
}



