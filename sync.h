#ifndef SYNC_H
#define SYNC_H

#include <mutex>

extern std::mutex timerValueMutex;
extern int64_t timerValue;

void syncTask();

#endif

