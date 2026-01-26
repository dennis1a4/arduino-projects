#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "storage.h"
#include "control.h"

// ============================================================================
// SCHEDULE MANAGER WITH NTP TIME SYNC
// ============================================================================

class ScheduleManager {
private:
    ConfigManager* _config;
    ThermostatController* _controller;
    bool _timeValid;
    unsigned long _lastNtpSync;
    int _activeScheduleIndex;
    bool _scheduleActive;
    String _scheduleInfo;

public:
    ScheduleManager(ConfigManager* config, ThermostatController* controller)
        : _config(config),
          _controller(controller),
          _timeValid(false),
          _lastNtpSync(0),
          _activeScheduleIndex(-1),
          _scheduleActive(false) {}

    void begin() {
        // Configure time with timezone
        // Using POSIX timezone format for America/Winnipeg (CST/CDT)
        configTime(-6 * 3600, 3600, "pool.ntp.org", "time.nist.gov");

        Serial.println(F("NTP time sync initialized"));
    }

    void update() {
        // Check if time is valid
        time_t now = time(nullptr);
        if (now > 1700000000) {  // After Nov 2023 - reasonable time
            _timeValid = true;
        }

        if (!_timeValid) return;

        // Get current time components
        struct tm* timeinfo = localtime(&now);
        if (!timeinfo) return;

        int currentDay = timeinfo->tm_wday;  // 0 = Sunday
        int currentHour = timeinfo->tm_hour;
        int currentMinute = timeinfo->tm_min;
        int currentMinutes = currentHour * 60 + currentMinute;

        // Check all schedules
        _activeScheduleIndex = -1;
        _scheduleActive = false;

        float floorTargetOverride = -1;
        float airTargetOverride = -1;

        for (int i = 0; i < MAX_SCHEDULES; i++) {
            Schedule& sched = _config->schedules[i];

            if (!sched.enabled) continue;

            // Check if this day is in the schedule
            if (!(sched.days & (1 << currentDay))) continue;

            // Check time range
            int startMinutes = sched.startHour * 60 + sched.startMinute;
            int endMinutes = sched.endHour * 60 + sched.endMinute;

            bool inTimeRange = false;

            if (startMinutes <= endMinutes) {
                // Normal range (e.g., 08:00 - 17:00)
                inTimeRange = (currentMinutes >= startMinutes && currentMinutes < endMinutes);
            } else {
                // Overnight range (e.g., 22:00 - 06:00)
                inTimeRange = (currentMinutes >= startMinutes || currentMinutes < endMinutes);
            }

            if (inTimeRange) {
                _activeScheduleIndex = i;
                _scheduleActive = true;

                // Apply target temperature based on zone
                if (sched.zone == ZONE_FLOOR) {
                    floorTargetOverride = sched.targetTemp;
                } else {
                    airTargetOverride = sched.targetTemp;
                }

                // Build schedule info string
                char buf[32];
                snprintf(buf, sizeof(buf), "%02d:%02d-%02d:%02d %.0f%s",
                         sched.startHour, sched.startMinute,
                         sched.endHour, sched.endMinute,
                         sched.targetTemp,
                         _config->system.useFahrenheit ? "F" : "C");
                _scheduleInfo = String(buf);
            }
        }

        // Apply schedule overrides (only if not in manual override)
        if (floorTargetOverride >= 0 &&
            _config->zones[ZONE_FLOOR].override == OVERRIDE_AUTO) {
            _controller->setEffectiveTarget(ZONE_FLOOR, floorTargetOverride);
        }

        if (airTargetOverride >= 0 &&
            _config->zones[ZONE_AIR].override == OVERRIDE_AUTO) {
            _controller->setEffectiveTarget(ZONE_AIR, airTargetOverride);
        }
    }

    void syncNTP() {
        if (!_timeValid) {
            Serial.println(F("Waiting for NTP sync..."));
        }
        _lastNtpSync = millis();
    }

    bool isTimeValid() const {
        return _timeValid;
    }

    bool isScheduleActive() const {
        return _scheduleActive;
    }

    int getActiveScheduleIndex() const {
        return _activeScheduleIndex;
    }

    const String& getScheduleInfo() const {
        return _scheduleInfo;
    }

    // Get current time as string
    String getCurrentTimeString() const {
        if (!_timeValid) return "No time";

        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);

        char buf[32];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        return String(buf);
    }

    // Get current date as string
    String getCurrentDateString() const {
        if (!_timeValid) return "No date";

        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);

        char buf[32];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                 timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
        return String(buf);
    }

    // Get day of week
    int getDayOfWeek() const {
        if (!_timeValid) return -1;

        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        return timeinfo->tm_wday;
    }

    static const char* getDayName(int day) {
        const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        if (day >= 0 && day <= 6) return days[day];
        return "???";
    }

    static const char* getDayNameFull(int day) {
        const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                              "Thursday", "Friday", "Saturday"};
        if (day >= 0 && day <= 6) return days[day];
        return "Unknown";
    }

    // Get uptime in seconds
    unsigned long getUptimeSeconds() const {
        return millis() / 1000;
    }

    // Format uptime string
    String getUptimeString() const {
        unsigned long secs = getUptimeSeconds();
        unsigned long mins = secs / 60;
        unsigned long hours = mins / 60;
        unsigned long days = hours / 24;

        char buf[32];
        if (days > 0) {
            snprintf(buf, sizeof(buf), "%lud %luh %lum", days, hours % 24, mins % 60);
        } else if (hours > 0) {
            snprintf(buf, sizeof(buf), "%luh %lum %lus", hours, mins % 60, secs % 60);
        } else {
            snprintf(buf, sizeof(buf), "%lum %lus", mins, secs % 60);
        }
        return String(buf);
    }

    // Check if a specific schedule would be active at a given time
    bool isScheduleActiveAt(int schedIndex, int dayOfWeek, int hour, int minute) const {
        if (schedIndex < 0 || schedIndex >= MAX_SCHEDULES) return false;

        const Schedule& sched = _config->schedules[schedIndex];

        if (!sched.enabled) return false;
        if (!(sched.days & (1 << dayOfWeek))) return false;

        int currentMinutes = hour * 60 + minute;
        int startMinutes = sched.startHour * 60 + sched.startMinute;
        int endMinutes = sched.endHour * 60 + sched.endMinute;

        if (startMinutes <= endMinutes) {
            return (currentMinutes >= startMinutes && currentMinutes < endMinutes);
        } else {
            return (currentMinutes >= startMinutes || currentMinutes < endMinutes);
        }
    }
};

#endif // SCHEDULER_H
