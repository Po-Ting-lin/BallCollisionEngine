#include "BallCollisionEngine.h"


BallCollisionEngine::BallCollisionEngine(int width, int height, int ballNum) {
	_screenHeight = height;
	_screenWidth = width;
	_ballNum = ballNum;
	_timeSplit = 4.0f;
	_minVelocity = 0.1f;
	_gravity = 0.0f;
}

BallCollisionEngine::~BallCollisionEngine() {
	Balls.clear();
	Lines.clear();
	_fakeBalls.clear();
	_collisionList.clear();
}


void BallCollisionEngine::Create() {
	//for (int i = 0; i < _ballNum; i++) {
	//	_addBall(rand() % _screenWidth, rand() % _screenHeight, rand() % 12 + 5);
	//}

	for (int y = _screenHeight * 0.3f; y < _screenHeight * 0.7f; y += 14.0f) {
		for (int x = _screenWidth * 0.3f; x < _screenWidth * 0.7f; x += 14.0f) {
			_addBall(x, y, 7.0f);
		}
	}

	//_addLine(30.0f, 30.0f, 30.0f, 920.0f, 4.0f);
	//_addLine(1000.0f, 30.0f, 1000.0f, 920.0f, 4.0f);
	//_addLine(30.0f, 920.0f, 1000.0f, 920.0f, 4.0f);
	_collisionList.reserve(_ballNum * _ballNum);
}

void BallCollisionEngine::Update(float& fElapsedTime) {
	// Break up the frame elapsed time into smaller deltas for each simulation update
	float simElapsedTime = fElapsedTime / _timeSplit;
	for (int i = 0; i < _timeSplit; i++)
	{
		// Set all balls time to maximum for this epoch
		for (auto& ball : Balls)
			ball.remainingTime = simElapsedTime;

		_updatePositionByVelocity(fElapsedTime);
		_collision();
	}
}

void BallCollisionEngine::_updatePositionByVelocity(float& fElapsedTime) {
#pragma omp parallel for
	for (int i = 0; i < Balls.size(); i++)
	{
		if (Balls[i].remainingTime > 0) {
			// start position
			Balls[i].sx = Balls[i].x;
			Balls[i].sy = Balls[i].y;

			// Add Drag to emulate rolling friction
			Balls[i].ax = -Balls[i].vx * 0.8f;
			Balls[i].ay = -Balls[i].vy * 0.8f + _gravity;

			// Update ball physics
			Balls[i].vx += Balls[i].ax * Balls[i].remainingTime;
			Balls[i].vy += Balls[i].ay * Balls[i].remainingTime;
			Balls[i].x += Balls[i].vx * Balls[i].remainingTime;
			Balls[i].y += Balls[i].vy * Balls[i].remainingTime;

			// Wrap the balls around screen
			if (Balls[i].x < 0) Balls[i].x += (float)_screenWidth;
			if (Balls[i].x >= _screenWidth) Balls[i].x -= (float)_screenWidth;
			if (Balls[i].y < 0) Balls[i].y += (float)_screenHeight;
			if (Balls[i].y >= _screenHeight) Balls[i].y -= (float)_screenHeight;

			// Clamp velocity near zero
			if (fabs(Balls[i].vx * Balls[i].vx + Balls[i].vy * Balls[i].vy) < _minVelocity)
			{
				Balls[i].vx = 0;
				Balls[i].vy = 0;
			}
		}
	}
}

void BallCollisionEngine::_collision() {
	_staticCollision();
	_dynamicCollision();
}


void BallCollisionEngine::_staticCollision() {
	for (auto& ball : Balls) {
		// collide with line
		for (auto& line : Lines) {
			float line_vector_x = line.ex - line.sx;
			float line_vector_y = line.ey - line.sy;
			float line_vector_sx = ball.x - line.sx;
			float line_vector_sy = ball.y - line.sy;
			float length = line_vector_x * line_vector_x + line_vector_y * line_vector_y;
			float t = std::max(0.0f, std::min(length, (line_vector_x * line_vector_sx + line_vector_y * line_vector_sy))) / length;
			float closest_x = line.sx + t * line_vector_x;
			float closest_y = line.sy + t * line_vector_y;
			float distance = sqrtf((ball.x - closest_x) * (ball.x - closest_x) + (ball.y - closest_y) * (ball.y - closest_y));

			if (distance <= (ball.r + line.r))
			{
				Ball fakeball;
				fakeball.r = line.r;
				fakeball.mass = ball.mass * 0.8f;
				fakeball.x = closest_x;
				fakeball.y = closest_y;
				fakeball.vx = -ball.vx;
				fakeball.vy = -ball.vy;
				_fakeBalls.push_back(fakeball);

				// Add collision to vector of collisions for dynamic resolution
				_collisionList.push_back({ &ball, &fakeball });

				// Calculate displacement required
				float fOverlap = 1.0f * (distance - ball.r - fakeball.r);

				// Displace Current Ball away from collision
				ball.x -= fOverlap * (ball.x - fakeball.x) / distance;
				ball.y -= fOverlap * (ball.y - fakeball.y) / distance;
			}
		}

		// collide with ball
		for (auto& target : Balls) {
			if (ball.id != target.id) {
				if (_isCollision(ball.x, ball.y, ball.r, target.x, target.y, target.r)) {
					_collisionList.push_back({ &ball, &target });

					// Distance between ball centers
					float fDistance = sqrtf((ball.x - target.x) * (ball.x - target.x) + (ball.y - target.y) * (ball.y - target.y));

					// Calculate displacement required
					float fOverlap = 0.5f * (fDistance - ball.r - target.r);

					// Displace Current Ball away from collision
					ball.x -= fOverlap * (ball.x - target.x) / fDistance;
					ball.y -= fOverlap * (ball.y - target.y) / fDistance;

					// Displace Target Ball away from collision
					target.x += fOverlap * (ball.x - target.x) / fDistance;
					target.y += fOverlap * (ball.y - target.y) / fDistance;
				}
			}
		}
		float fIntendedSpeed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
		float fIntendedDistance = fIntendedSpeed * ball.remainingTime;
		float fActualDistance = sqrtf((ball.x - ball.sx) * (ball.x - ball.sx) + (ball.y - ball.sy) * (ball.y - ball.sy));
		float fActualTime = fActualDistance / fIntendedSpeed;
		ball.remainingTime -= fActualTime;
	}
}

void BallCollisionEngine::_dynamicCollision() {
	for (auto c : _collisionList)
	{
		Ball* b1 = c.first;
		Ball* b2 = c.second;

		// Distance between balls
		float diff_x = b1->x - b2->x;
		float diff_y = b1->y - b2->y;

		float fDistance = sqrtf(diff_x * diff_x + diff_y * diff_y);

		// Normal
		float nx = diff_x / fDistance;
		float ny = diff_y / fDistance;

		// Wikipedia Version - Maths is smarter but same
		float p = 2.0 * (nx * (b1->vx - b2->vx) + ny * (b1->vy - b2->vy)) / (b1->mass + b2->mass);
		b1->vx = b1->vx - p * b2->mass * nx;
		b1->vy = b1->vy - p * b2->mass * ny;
		b2->vx = b2->vx + p * b1->mass * nx;
		b2->vy = b2->vy + p * b1->mass * ny;
	}
	_collisionList.clear();
	_fakeBalls.clear();
}

