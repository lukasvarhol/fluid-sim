#pragma once
#include <random>
#include <array>

inline float randomFloat(float lowerbound) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(lowerbound, 1.0f);

    return dist(gen);
}

inline std::array<float,3> rgba_normalizer(const int r, const int g, const int b)
{
    return {
        r / 255.0f,
        g / 255.0f,
        b / 255.0f
        };
}