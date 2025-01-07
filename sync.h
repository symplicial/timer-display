#ifndef SYNC_H
#define SYNC_H

#include <mutex>
#include <chrono>

enum TimerPhase {
    NotRunning,
    Running,
    Ended,
    Paused
};

extern std::mutex endMutex;
extern bool end;

extern std::mutex timerValueMutex;
extern std::chrono::time_point<std::chrono::system_clock> timerValueLastUpdate;
extern int64_t timerValue;
void timerValueSyncTask();

extern std::mutex timerPhaseMutex;
extern TimerPhase timerPhase;
void timerPhaseSyncTask();

extern std::mutex deltaMutex;
extern bool hasDelta;
extern int64_t delta;
void deltaSyncTask();

extern std::mutex pbSplitTimeMutex;
extern bool hasPbSplitTime;
extern int64_t pbSplitTime;
void pbSplitTimeSyncTask();

extern std::mutex sobMutex;
extern int64_t sob;
void sobSyncTask();

extern std::mutex bptMutex;
extern int64_t bpt;
void bptSyncTask();

extern std::mutex bestDeltaMutex;
extern bool isGold;
void bestDeltaSyncTask();

#endif

