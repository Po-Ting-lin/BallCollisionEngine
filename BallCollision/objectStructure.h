#pragma once

struct Ball {
	int id;
	float x, y;
	float sx, sy;
	float vx, vy;
	float ax, ay;
	float r;
	float mass;
	float remainingTime;
	float color;
};

struct Line {
	int id;
	float sx, sy;
	float ex, ey;
	float r;
};