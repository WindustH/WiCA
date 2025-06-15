#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For default ostream in printReport

class Timer {
public:
    // 定义需要计时的模块
    enum class Module {
        // --- 示例模块 ---
        calculateForUpdate,
        applyUpdate,
        renderGrid,
        SDL_RenderFillRect,
        renderCells,
        // 必须的：用于迭代或作为未知模块的标记
        __MODULE_COUNT__ // 可以用来获取模块数量，但通常不直接用作计时模块
    };

    // 工厂方法获取一个 Timer 实例
    // 每个 Timer 实例用于一次 start-stop 周期
    static Timer getTimer(Module module);

    // 构造函数（公开，以便直接创建，但推荐使用 getTimer）
    explicit Timer(Module module);

    // Timer 不可复制或移动，以避免意外行为
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    // 开始计时
    void start();

    // 停止计时并将持续时间添加到模块的全局累积时间
    void stop();

    // --- 静态方法用于全局统计 ---

    // 获取所有模块的累积时间（单位：毫秒）
    // 返回的是一个副本，确保线程安全和数据隔离
    static std::map<Module, std::chrono::duration<double, std::milli>> getAccumulatedTimes();

    // 打印所有模块的累积时间报告
    static void printReport(std::ostream& os = std::cout);

    // 重置所有模块的累积时间
    static void resetAll();

    // 将模块枚举转换为字符串（用于报告）
    static std::string moduleToString(Module module);

private:
    Module module_;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTimePoint_;
    bool isRunning_ = false;
    bool hasBeenStopped_ = false; // 标记此实例是否已调用过 stop()

    // 静态成员变量，用于存储所有模块的累积时间
    // 使用指针确保在首次使用前初始化，并管理生命周期（可选，直接静态成员也可以）
    static std::map<Module, std::chrono::duration<double, std::milli>> s_accumulatedTimes_;
    static std::mutex s_mutex_; // 用于保护对 s_accumulatedTimes_ 的访问
};

#endif // TIMER_H