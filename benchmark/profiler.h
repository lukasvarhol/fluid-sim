#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <mutex>
#include <iostream>
#include <fstream>

enum Phase {
  GRAVITY_PREDICT,
  BUILD_GRID,
  BUILD_NEIGHBOURS,
  SOLVER,
  VELOCITY_UPDATE,
  VISCOSITY,
  VORTICITY
};

static const char *EnumToString[] = {
    "gravity_predict", "build_grid", "build_nieghbours", "solver",
    "velocity_update", "viscosity", "vorticity"
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
  static std::mutex mtx;
  Profiler(const Profiler&) = delete;
  static Profiler &Get() { return instance_; }
  static std::vector<TimeCouple> &GetTimerManager() { return timerManager_; }
  void Init(const std::string &filepath, const int numParticles,
            const int numFrames, const std::string backend, const std::string commit) {
    filepath_ = filepath;
    numParticles_ = numParticles;
    numFrames_ = numFrames;
    backend_ = backend;
    commit_ = commit;
    timerManager_.reserve(dataSize);
  }

  void Write() {
    std::ofstream out(filepath_.c_str());
    out << "commit,backend,particle_count,phase,frame,elapsed_us" << std::endl;
    for (TimeCouple timer : timerManager_) {
      out << commit_ << backend_
          << numParticles_
          << EnumToString[timer.phase] << "," << timer.frame << ","
          << (std::chrono::duration_cast<std::chrono::microseconds>(
								    timer.stopTime - timer.startTime))
	.count()
	  << std::endl;
    }
  }
  
 private:
  Profiler(){}
  static Profiler instance_;
  static std::string filepath_;
  static int numParticles_;
  static int numFrames_;
  static std::string backend_;
  static std::string commit_;
  int dataSize = numFrames_ * 7;
  static std::vector<TimeCouple> timerManager_; 

};

class Profiler::Timer {
 public:
  Timer(Phase phase, unsigned int frame) {
    auto startTime = std::chrono::steady_clock::now();
    std::vector<TimeCouple>& timerManager = Profiler::GetTimerManager();
    Profiler::mtx.lock();
    timerManager.push_back(TimeCouple{startTime, startTime, phase, frame});
    timerId = timerManager.size() - 1;
    Profiler::mtx.unlock();
  }
  ~Timer() {
    std::vector<TimeCouple> &timerManager = Profiler::GetTimerManager();
    Profiler::mtx.lock();
    timerManager.at(timerId).stopTime = std::chrono::steady_clock::now();
    Profiler::mtx.unlock();
  }

 private:
  size_t timerId;
};
  
