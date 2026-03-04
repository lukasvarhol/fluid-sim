#include "cell.h"

Cell& Cell::operator+=(const Cell& other) {
	x += other.x;
	y += other.y;
}

Cell Cell::operator+(const Cell& other) const {
	Cell result;
	result += other;
	return result;
}