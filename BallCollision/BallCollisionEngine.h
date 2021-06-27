#pragma once
#include <vector>
#include <omp.h>
#include "utils.h"
#include "objectStructure.h"


class BallCollisionEngine {
public:
	BallCollisionEngine(int width, int height, int ballNum);
	~BallCollisionEngine();

	void Create();
	void Update(float& fElapsedTime);

	std::vector<Ball> Balls;
	std::vector<Line> Lines;
private:
	std::vector<Ball> _fakeBalls;
	std::vector<std::pair<Ball*, Ball*>> _collisionList;
	int _ballNum;
	int _screenWidth;
	int _screenHeight;
	float _timeSplit;
	float _minVelocity;
	float _gravity;

	inline bool _isCollision(float x1, float y1, float r1, float x2, float y2, float r2) {
		return fabs((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)) <= (r1 + r2) * (r1 + r2);
	}
	inline void _addBall(float x, float y, float radius) {
		Ball ball;
		ball.id = Balls.size();
		ball.x = x;
		ball.y = y;
		ball.vx = 0;
		ball.vy = 0;
		ball.ax = 0;
		ball.ay = 0;
		ball.r = radius;
		ball.mass = 10.0f * radius;
		Balls.emplace_back(ball);
	}
	inline void _addLine(float sx, float sy, float ex, float ey, float radius) {
		Line line;
		line.sx = sx;
		line.sy = sy;
		line.ex = ex;
		line.ey = ey;
		line.r = radius;
		Lines.emplace_back(line);
	}

	void _updatePositionByVelocity(float& fElapsedTime);
	void _collision();
	void _staticCollision();
	void _dynamicCollision();
};

