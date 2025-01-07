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

std::mutex endMutex;
bool end = false;

std::mutex timerValueMutex;
std::chrono::time_point<std::chrono::system_clock> timerValueLastUpdate;
int64_t timerValue;
int timerValueUncertainty = -99999999;

std::mutex timerPhaseMutex;
TimerPhase timerPhase = TimerPhase::NotRunning;

std::mutex deltaMutex;
bool hasDelta = false;
int64_t delta = 0;

std::mutex pbSplitTimeMutex;
bool hasPbSplitTime = false;
int64_t pbSplitTime = 0;

std::mutex sobMutex;
int64_t sob;

std::mutex bptMutex;
int64_t bpt;

std::mutex bestDeltaMutex;
bool isGold = false;
bool hasPreviousBestDelta = false;
int64_t previousBestDelta = 0;
bool hasBestDelta = false;
int64_t bestDelta = 0;

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

TimerPhase parseTimerPhase(std::string str) {
    if (str == "NotRunning\n")
        return TimerPhase::NotRunning;
    else if (str == "Running\n")
        return TimerPhase::Running;
    else if (str == "Ended\n")
        return TimerPhase::Ended;
    else if (str == "Paused\n")
        return TimerPhase::Paused;
    return TimerPhase::NotRunning;
}

void timerValueSyncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        auto t1 = std::chrono::system_clock::now();
        const char *msg = "getcurrenttime\r\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        const auto t2 = std::chrono::system_clock::now();

        int newUncertainty = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        buf[n] = '\0';

        int64_t newTimerValue = parseTimespan(std::string(buf));

        /* If the timer is not running, we are 100% certain this is the actual value. */
        TimerPhase phase;
        {
            std::lock_guard<std::mutex> guard(timerPhaseMutex);
            phase = timerPhase;
        }

        /* Only change the value if the new value cannot be explained by the lengths of the requests, i.e. a reset, pause, or correcting a big error. */
        {
            std::lock_guard<std::mutex> guard(timerValueMutex);
            if (phase == TimerPhase::Running) {
                int correction = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - timerValueLastUpdate).count();
                int64_t correctedTimerValue = timerValue + correction;
                if (abs(correctedTimerValue - newTimerValue) >= timerValueUncertainty + newUncertainty + 5) {
                    timerValue = newTimerValue;
                    timerValueUncertainty = newUncertainty;
                    timerValueLastUpdate = std::chrono::system_clock::now();
                }
            } else {
                timerValue = newTimerValue;
                timerValueUncertainty = -9999999;
                timerValueLastUpdate = std::chrono::system_clock::now();
            }
        }

        t1 += std::chrono::milliseconds(100);
        std::this_thread::sleep_until(t1);
    }

    close(sock);
}

void timerPhaseSyncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        auto t1 = std::chrono::system_clock::now();
        const char *msg = "getcurrenttimerphase\r\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        buf[n] = '\0';
        TimerPhase newPhase = parseTimerPhase(std::string(buf));
        
        {
            std::lock_guard<std::mutex> guard(timerPhaseMutex);
            timerPhase = newPhase;
        }

        t1 += std::chrono::milliseconds(100);
        std::this_thread::sleep_until(t1);
    }

    close(sock);
}

void sobSyncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        auto t1 = std::chrono::system_clock::now();
        const char *msg = "getfinaltime Best Segments\r\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        buf[n] = '\0';

        int64_t newSob = parseTimespan(std::string(buf));

        {
            std::lock_guard<std::mutex> guard(sobMutex);
            sob = newSob;
        }

        t1 += std::chrono::milliseconds(1000);
        std::this_thread::sleep_until(t1);
    }

    close(sock);
}

void bptSyncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        auto t1 = std::chrono::system_clock::now();
        const char *msg = "getbestpossibletime\r\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        buf[n] = '\0';

        int64_t newBpt = parseTimespan(std::string(buf));

        {
            std::lock_guard<std::mutex> guard(bptMutex);
            bpt = newBpt;
        }

        t1 += std::chrono::milliseconds(1000);
        std::this_thread::sleep_until(t1);
    }

    close(sock);
}

void deltaSyncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        auto t1 = std::chrono::system_clock::now();
        const char *msg = "getdelta Personal Best\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        buf[n] = '\0';

        std::string result(buf);
        if (result == "-\n") {
            std::lock_guard<std::mutex> guard(deltaMutex);
            hasDelta = false;
        } else {
            std::lock_guard<std::mutex> guard(deltaMutex);
            delta = parseTimespan(result);
            hasDelta = true;
        }

        t1 += std::chrono::milliseconds(100);
        std::this_thread::sleep_until(t1);
    }

    close(sock);
}

void pbSplitTimeSyncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        auto t1 = std::chrono::system_clock::now();
        const char *msg = "getcomparisonsplittime Personal Best\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        buf[n] = '\0';

        std::string result(buf);
        if (result == "-\n") {
            std::lock_guard<std::mutex> guard(pbSplitTimeMutex);
            hasPbSplitTime = false;
        } else {
            std::lock_guard<std::mutex> guard(pbSplitTimeMutex);
            pbSplitTime = parseTimespan(result);
            hasPbSplitTime = true;
        }

        t1 += std::chrono::milliseconds(100);
        std::this_thread::sleep_until(t1);
    }

    close(sock);
}

void bestDeltaSyncTask() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(16834);
    inet_pton(AF_INET, "192.168.1.115", &server.sin_addr);

    int res = connect(sock, (const sockaddr *)&server, sizeof(server));

    while (true) {
        {
            std::lock_guard<std::mutex> guard(endMutex);
            if (end)
                break;
        }

        auto t1 = std::chrono::system_clock::now();
        const char *msg = "getdelta Best Segments\n";
        send(sock, msg, strlen(msg), 0);

        char buf[64];
        int n;
        n = recv(sock, buf, 63, 0);
        buf[n] = '\0';

        std::string result(buf);
        {
            std::lock_guard<std::mutex> guard(bestDeltaMutex);
            hasPreviousBestDelta = hasBestDelta;
            previousBestDelta = bestDelta;
            if (result == "-\n") {
                hasBestDelta = false;
            } else {
                bestDelta = parseTimespan(result);
                hasBestDelta = true;
            }
            /* A gold occurs when the delta vs best segments decreases (or it is negative). It goes away if the delta increases or there is no delta. */
            if ((hasPreviousBestDelta && hasBestDelta && bestDelta < previousBestDelta) || (hasBestDelta && bestDelta < 0))
                isGold = true;
            if (!hasBestDelta || (hasBestDelta && hasPreviousBestDelta && bestDelta > previousBestDelta))
                isGold = false;
        }

        t1 += std::chrono::milliseconds(100);
        std::this_thread::sleep_until(t1);
    }

    close(sock);
}

