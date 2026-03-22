#include "scheduled_tasks.h"
#include "globals.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <time.h>

std::vector<ScheduledTask> scheduledTasks;

static const char* TASKS_FILE = "/sched_tasks.json";
static int nextId = 1;

void loadScheduledTasks() {
    scheduledTasks.clear();
    nextId = 1;
    if (!SPIFFS.exists(TASKS_FILE)) return;
    File f = SPIFFS.open(TASKS_FILE, "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    f.close();
    JsonArray arr = doc["tasks"].as<JsonArray>();
    for (JsonObject o : arr) {
        ScheduledTask t;
        t.id      = o["id"]      | 0;
        t.year    = o["year"]    | 0;
        t.month   = o["month"]   | 0;
        t.day     = o["day"]     | 0;
        t.hour    = o["hour"]    | 0;
        t.minute  = o["minute"]  | 0;
        t.second  = o["second"]  | 0;
        t.payload = o["payload"].as<String>();
        t.label   = o["label"].as<String>();
        t.enabled = o["enabled"] | true;
        t.repeat  = o["repeat"]  | false;
        t.fired   = false; // reset on reboot
        if (t.id >= nextId) nextId = t.id + 1;
        scheduledTasks.push_back(t);
    }
}

void saveScheduledTasks() {
    JsonDocument doc;
    JsonArray arr = doc["tasks"].to<JsonArray>();
    for (const ScheduledTask& t : scheduledTasks) {
        JsonObject o = arr.add<JsonObject>();
        o["id"]      = t.id;
        o["year"]    = t.year;
        o["month"]   = t.month;
        o["day"]     = t.day;
        o["hour"]    = t.hour;
        o["minute"]  = t.minute;
        o["second"]  = t.second;
        o["payload"] = t.payload;
        o["label"]   = t.label;
        o["enabled"] = t.enabled;
        o["repeat"]  = t.repeat;
    }
    File f = SPIFFS.open(TASKS_FILE, "w");
    if (!f) return;
    serializeJson(doc, f);
    f.close();
}

int addScheduledTask(const ScheduledTask& t) {
    ScheduledTask nt = t;
    nt.id    = nextId++;
    nt.fired = false;
    scheduledTasks.push_back(nt);
    saveScheduledTasks();
    return nt.id;
}

bool deleteScheduledTask(int id) {
    for (auto it = scheduledTasks.begin(); it != scheduledTasks.end(); ++it) {
        if (it->id == id) { scheduledTasks.erase(it); saveScheduledTasks(); return true; }
    }
    return false;
}

bool updateScheduledTask(const ScheduledTask& t) {
    for (ScheduledTask& e : scheduledTasks) {
        if (e.id == t.id) { e = t; e.fired = false; saveScheduledTasks(); return true; }
    }
    return false;
}

void checkScheduledTasks() {
    if (scheduledTasks.empty()) return;
    time_t now = time(nullptr);
    if (now < 100000) return; // NTP not synced

    // Only check once per second at most
    static time_t lastChecked = 0;
    if (now == lastChecked) return;
    lastChecked = now;

    struct tm* tm_ = localtime(&now);
    bool changed = false;

    for (ScheduledTask& t : scheduledTasks) {
        if (!t.enabled) continue;

        bool match = true;
        if (t.year   > 0 && t.year   != (tm_->tm_year + 1900)) match = false;
        if (t.month  > 0 && t.month  != (tm_->tm_mon  + 1))    match = false;
        if (t.day    > 0 && t.day    != tm_->tm_mday)           match = false;
        if (t.hour        != tm_->tm_hour)                       match = false;
        if (t.minute      != tm_->tm_min)                        match = false;
        // Match if current second >= task second so short-circuit second=0 always
        // matches for the entire target minute after HH:MM:00
        if (tm_->tm_sec < t.second)                              match = false;

        if (match && !t.fired) {
            t.fired = true;
            changed = true;
            if (t.payload.length() > 0)
                pendingTokenStrings.push_back(t.payload);
            if (!t.repeat)
                t.enabled = false;
        } else if (!match && t.fired) {
            if (t.repeat) t.fired = false;
        }
    }

    if (changed) saveScheduledTasks();
}
