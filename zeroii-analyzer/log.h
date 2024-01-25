#ifndef _LOG_H
#define _LOG_H

//#define DISABLE_LOG

#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_INFO 3
#define LOG_DEBUG 4
#define LOG_TRACE 5

const char* level_names[] = {"CRIT", "ERROR", "WARN", "INFO", "DEBUG", "TRACE" };

class Logger {
public:
    Logger(const char* name) {
#ifndef DISABLE_LOG
        name_ = String(name);
        level_ = LOG_INFO;
#endif
    }

    Logger(const char* name, uint8_t level) {
#ifndef DISABLE_LOG
        name_ = String(name);
        level_ = level;
#endif
    }

    void log(uint8_t level, const char* message) {
#ifndef DISABLE_LOG
        if(level_ >= level) {
            Serial.print("[");
            Serial.print(level_names[level]);
            Serial.print("]\t");
            Serial.print(name_);
            Serial.print("\t");
            Serial.println(message);
            Serial.flush();
        }
#endif
    }

    void log(uint8_t level, const __FlashStringHelper* message) {
#ifndef DISABLE_LOG
        if(level_ >= level) {
            Serial.print("[");
            Serial.print(level_names[level]);
            Serial.print("]\t");
            Serial.print(name_);
            Serial.print("\t");
            Serial.println(message);
            Serial.flush();
        }
#endif
    }

    void error(const char* message) {
        log(LOG_ERROR, message);
    }
    void warn(const char* message) {
        log(LOG_WARNING, message);
    }
    void info(const char* message) {
        log(LOG_INFO, message);
    }
    void debug(const char* message) {
        log(LOG_DEBUG, message);
    }

    void error(const __FlashStringHelper* message) {
        log(LOG_ERROR, message);
    }
    void warn(const __FlashStringHelper* message) {
        log(LOG_WARNING, message);
    }
    void info(const __FlashStringHelper* message) {
        log(LOG_INFO, message);
    }
    void debug(const __FlashStringHelper* message) {
        log(LOG_DEBUG, message);
    }

    void log(uint8_t level, String message) {
        log(level, message.c_str());
    }
    void error(String message) {
        error(message.c_str());
    }
    void warn(String message) {
        warn(message.c_str());
    }
    void info(String message) {
        info(message.c_str());
    }
    void debug(String message) {
        debug(message.c_str());
    }

private:
#ifndef DISABLE_LOG
    String name_;
    uint8_t level_;
#endif
};

#endif
