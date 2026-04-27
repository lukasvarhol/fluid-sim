#include "cell.h"

Cell &Cell::operator+=(const Cell &other)
{
	x += other.x;
        y += other.y;
	z += other.z;
	return *this;
}

Cell Cell::operator+(const Cell &other) const
{
	Cell result;
	result += other;
	return result;
}
