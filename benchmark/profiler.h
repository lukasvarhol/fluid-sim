#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <mutex>

enum Phase {
  GRAVITY_PREDICT,
  BUILD_GRID,
  BUILD_NEIGHBOURS,
  SOLVER,
  VELOCITY_UPDATE,
  VISCOSITY,
  VORTICITY
};

struct TimeCouple {
  std::chrono::time_point<std::chrono::steady_clock> startTime;
  std::chrono::time_point<std::chrono::steady_clock> stopTime;
  Phase phase;
  unsigned int frame;
};
  
class Profiler {
 public:
  class Timer;
  Profiler(const Profiler&) = delete;
  static Profiler &get() { return instance_; }
  static std::vector<TimeCouple> &getTimerManager() { return timerManager_; }
  static std::mutex mtx;

 private:
  Profiler(){}
  static Profiler instance_;
  static std::vector<TimeCouple> timerManager_; 

};

class Profiler::Timer {
 public:
  Timer(Phase phase, unsigned int frame) {
    auto startTime = std::chrono::steady_clock::now();
    std::vector<TimeCouple>& timerManager = Profiler::getTimerManager();
    Profiler::mtx.lock();
    timerManager.push_back(TimeCouple{startTime, startTime, phase, frame});
    timerId = timerManager.size() - 1;
    Profiler::mtx.unlock();
  }
  ~Timer() {
    std::vector<TimeCouple> &timerManager = Profiler::getTimerManager();
    Profiler::mtx.lock();
    timerManager.at(timerId).stopTime = std::chrono::steady_clock::now();
    Profiler::mtx.unlock();
  }

 private:
  size_t timerId;
};
  
