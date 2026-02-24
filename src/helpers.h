#pragma once
#include <random>

inline float randomFloat(float lowerbound) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(lowerbound, 1.0f);

    return dist(gen);
}