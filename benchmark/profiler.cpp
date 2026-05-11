#include "profiler.h"

std::string Profiler::filepath_ = "";
std::string Profiler::commit_ = "";
std::string Profiler::backend_ = "";
int Profiler::numParticles_ = 0;
int Profiler::numFrames_ = 0;
std::vector<TimeCouple> Profiler::timerManager_;
std::mutex Profiler::mtx;
