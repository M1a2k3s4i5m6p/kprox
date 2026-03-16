#pragma once
#include <Arduino.h>
#include <vector>

struct ScheduledTask {
    int    id;
    int    year;    // 0 = any
    int    month;   // 0 = any, 1-12
    int    day;     // 0 = any, 1-31
    int    hour;    // 0-23
    int    minute;  // 0-59
    int    second;  // 0-59
    String payload; // token string to execute
    String label;   // human-readable name
    bool   enabled;
    bool   fired;   // true after firing once (for one-shot tasks)
    bool   repeat;  // true = re-arm after firing
};

extern std::vector<ScheduledTask> scheduledTasks;

void loadScheduledTasks();
void saveScheduledTasks();
void checkScheduledTasks();
int  addScheduledTask(const ScheduledTask& t);
bool deleteScheduledTask(int id);
bool updateScheduledTask(const ScheduledTask& t);
