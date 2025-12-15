#pragma once

#include "Defines.hpp"

class Vec2 {
public:
	Vec2() : x(0.f), y(0.f) {}
	Vec2(real x, real y) : x(x), y(y) {}

	union {
		struct {
			real x; 
			real y;
		};

		real e[2];
	};
};