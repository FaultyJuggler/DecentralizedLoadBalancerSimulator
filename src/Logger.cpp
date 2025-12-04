#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>

Logger::Logger() : use_file_(false) {
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    log_file_.open(filename, std::ios::out | std::ios::app);
    use_file_ = log_file_.is_open();
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = getCurrentTimestamp();
    std::string log_message = "[" + timestamp + "] " + message;
    
    if (use_file_ && log_file_.is_open()) {
        log_file_ << log_message << std::endl;
    } else {
        std::cout << log_message << std::endl;
    }
}

void Logger::logNodeEvent(int node_id, const std::string& event) {
    std::string message = "Node[" + std::to_string(node_id) + "] " + event;
    log(message);
}

void Logger::logMetrics(int node_id, int current_load, int tasks_processed) {
    std::string message = "Node[" + std::to_string(node_id) + "] " +
                         "Load=" + std::to_string(current_load) + " " +
                         "TasksProcessed=" + std::to_string(tasks_processed);
    log(message);
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    
    return ss.str();
}
