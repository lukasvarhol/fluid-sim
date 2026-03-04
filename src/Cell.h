#pragma once

struct Cell {
	int x, y;

	Cell& operator+=(const Cell& other);
	Cell operator+(const Cell& other) const;
};

struct CellEntry {
	unsigned int hash;
	size_t index;
};