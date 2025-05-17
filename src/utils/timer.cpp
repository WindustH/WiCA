#include "Timer.h"
#include <iomanip>

std::map<Timer::Module, std::chrono::duration<double, std::milli>> Timer::s_accumulatedTimes_;
std::mutex Timer::s_mutex_;

Timer::Timer(Module module)
    : module_(module), isRunning_(false), hasBeenStopped_(false) {}

Timer Timer::getTimer(Module module) {
    return Timer(module);
}

void Timer::resetAll() {
    std::lock_guard<std::mutex> lock(s_mutex_);
    s_accumulatedTimes_.clear();
}

std::string Timer::moduleToString(Module module) {
    switch (module) {
        case Module::calculateForUpdate:   return "calculateForUpdate";
        case Module::applyUpdate:          return "applyUpdate";
        case Module::renderGrid:           return "renderGrid";
        case Module::SDL_RenderFillRect:   return "SDL_RenderFillRect";
        case Module::renderCells:          return "renderCells";
        default:                           return "UNKNOWN_MODULE";
    }
}

void Timer::start() {
    if (isRunning_) {
        throw std::runtime_error("Timer for module " + moduleToString(module_) + " is already running.");
    }
    if (hasBeenStopped_) {
        throw std::runtime_error("Timer for module " + moduleToString(module_) +
                                 " has already been stopped. Create a new Timer instance for a new measurement.");
    }
    startTimePoint_ = std::chrono::high_resolution_clock::now();
    isRunning_ = true;
}

void Timer::stop() {
    if (!isRunning_) {
        if (hasBeenStopped_) {
            return;
        }
        throw std::runtime_error("Timer for module " + moduleToString(module_) + " was not started.");
    }

    auto endTimePoint = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = endTimePoint - startTimePoint_;

    {
        std::lock_guard<std::mutex> lock(s_mutex_);
        s_accumulatedTimes_[module_] += duration;
    }

    isRunning_ = false;
    hasBeenStopped_ = true;
}

std::map<Timer::Module, std::chrono::duration<double, std::milli>> Timer::getAccumulatedTimes() {
    std::lock_guard<std::mutex> lock(s_mutex_);
    return s_accumulatedTimes_; // 返回一个副本
}

void Timer::printReport(std::ostream& os) {
    os << "Timer Report (accumulated times):\n";
    os << "------------------------------------\n";
    os << std::fixed << std::setprecision(3);

    std::map<Module, std::chrono::duration<double, std::milli>> timesToReport;
    {
        std::lock_guard<std::mutex> lock(s_mutex_);
        timesToReport = s_accumulatedTimes_;
    }

    if (timesToReport.empty()) {
        os << "No timing data recorded.\n";
    } else {
        for (const auto& pair : timesToReport) {
            os << "Module: " << std::left << std::setw(25) << moduleToString(pair.first)
               << " | Time: " << std::right << std::setw(10) << pair.second.count() << " ms\n";
        }
    }
    os << "------------------------------------\n";
}

