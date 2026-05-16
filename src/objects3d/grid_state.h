#pragma once
#include "linear_algebra.h"
#include <array>

constexpr int   GRID_X    = 5;
constexpr int   GRID_Y    = 5;
constexpr int   GRID_Z    = 5;
constexpr float CELL_SIZE = 2.0f / static_cast<float>(GRID_X); // 0.4

enum class Feature     { EMPTY, B_RAMP, T_RAMP, S_CHANNEL, L_CHANNEL };
static constexpr int NUM_FEATURES = 8;
enum class Orientation { North, East, South, West };
// Yaw mapping: North=0, East=PI/2, South=PI, West=3*PI/2

struct CellFeature {
    Feature     feature = Feature::EMPTY;
    Orientation facing  = Orientation::North;
    int         variant = 0; // Ramp: 0=highest step in cell, 4=lowest step
};

struct GridState {
    std::array<CellFeature, GRID_X * GRID_Y * GRID_Z> layout = {};
    int selX = 2, selY = 2, selZ = 2;

    int CellIndex(int x, int y, int z) const {
        return x + y * GRID_X + z * GRID_X * GRID_Y;
    }
    bool IsValidCell(int x, int y, int z) const {
        return x >= 0 && x < GRID_X &&
               y >= 0 && y < GRID_Y &&
               z >= 0 && z < GRID_Z;
    }
    Vec3 CellCenterWorld(int x, int y, int z) const {
        return {
            -1.0f + (x + 0.5f) * CELL_SIZE,
            -1.0f + (y + 0.5f) * CELL_SIZE,
            -1.0f + (z + 0.5f) * CELL_SIZE
        };
    }
    CellFeature& GetCell(int x, int y, int z) {
        return layout[CellIndex(x, y, z)];
    }
    const CellFeature& GetCell(int x, int y, int z) const {
        return layout[CellIndex(x, y, z)];
    }
};
