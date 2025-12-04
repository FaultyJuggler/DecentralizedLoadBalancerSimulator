#include "Task.h"
#include <thread>

Task::Task(int id, int complexity)
    : id_(id), complexity_(complexity), 
      creation_time_(std::chrono::steady_clock::now()) {
}

int Task::getId() const {
    return id_;
}

int Task::getComplexity() const {
    return complexity_;
}

std::chrono::steady_clock::time_point Task::getCreationTime() const {
    return creation_time_;
}

void Task::execute() {
    // Simulate task execution with sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(complexity_));
}
