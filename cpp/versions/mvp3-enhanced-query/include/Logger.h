#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <memory>

// [SEQUENCE: 33] Abstract logger interface
class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(const std::string& message) = 0;
    virtual void info(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;
};

// [SEQUENCE: 34] Console logger implementation
class ConsoleLogger : public Logger {
public:
    void log(const std::string& message) override;
    void info(const std::string& message) override;
    void error(const std::string& message) override;

private:
    void writeLog(const std::string& level, const std::string& message);
};

#endif // LOGGER_H