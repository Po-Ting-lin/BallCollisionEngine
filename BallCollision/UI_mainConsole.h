#pragma once
#include <omp.h>
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "objectStructure.h"
#include "utils.h"

#define PI 3.14159f

class BallCollision : public olc::PixelGameEngine
{
public:
	BallCollision() { 
		sAppName = "BallCollision";
		_timeSplit = 4.0f;
		_minVelocity = 0.1f;
		_gravity = 0.0f;
	}
private:
	std::vector<Ball> _balls;
	std::vector<Ball> _fakeBalls;
	std::vector<std::pair<Ball*, Ball*>> _collisionList;
	Ball* _selectedBall;
	float _timeSplit;
	float _minVelocity;

	std::vector<Line> _lines;
	Line* _selectedLine;
	bool _isStartLine;

	float _gravity;

	void _addBall(float x, float y, float radius) {
		Ball ball;
		ball.id = _balls.size();
		ball.x = x;
		ball.y = y;
		ball.vx = 0;
		ball.vy = 0;
		ball.ax = 0;
		ball.ay = 0;
		ball.r = radius;
		ball.mass = 10.0f * radius;
		_balls.emplace_back(ball);
	}

	void _addLine(float sx, float sy, float ex, float ey, float radius) {
		Line line;
		line.sx = sx;
		line.sy = sy;
		line.ex = ex;
		line.ey = ey;
		line.r = radius;
		_lines.emplace_back(line);
	}

	inline bool _isCollision(float x1, float y1, float r1, float x2, float y2, float r2) {
		return fabs((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)) <= (r1 + r2) * (r1 + r2);
	}

	inline bool _isPointInBall(float x1, float y1, float r1, float x, float y) {
		return fabs((x1 - x) * (x1 - x) + (y1 - y) * (y1 - y)) < (r1 * r1);
	}

	inline void _selectAndDragBall() {
		if (GetMouse(olc::Mouse::LEFT).bPressed || GetMouse(olc::Mouse::RIGHT).bPressed) {
			_selectedBall = nullptr;
			for (auto& ball : _balls) {
				if (_isPointInBall(ball.x, ball.y, ball.r, GetMouseX(), GetMouseY())) {
					_selectedBall = &ball; break;
				}
			}

			_selectedLine = nullptr;
			for (auto& line : _lines) {
				if (_isPointInBall(line.sx, line.sy, line.r, GetMouseX(), GetMouseY())) {
					_selectedLine = &line;
					_isStartLine = true;
				}
			}
			for (auto& line : _lines) {
				if (_isPointInBall(line.ex, line.ey, line.r, GetMouseX(), GetMouseY())) {
					_selectedLine = &line;
					_isStartLine = false;
				}
			}
		}

		if (GetMouse(olc::Mouse::LEFT).bHeld) {
			if (_selectedBall) {
				_selectedBall->x = GetMouseX();
				_selectedBall->y = GetMouseY();
			}

			if (_selectedLine && _isStartLine) {
				_selectedLine->sx = GetMouseX();
				_selectedLine->sy = GetMouseY();
			}
			if (_selectedLine && !_isStartLine) {
				_selectedLine->ex = GetMouseX();
				_selectedLine->ey = GetMouseY();
			}
		}

		if (GetMouse(olc::Mouse::LEFT).bReleased) {
			_selectedBall = nullptr;
			_selectedLine = nullptr;
		}

		if (GetMouse(olc::Mouse::RIGHT).bReleased) {
			if (_selectedBall) {
				_selectedBall->vx = 20.0f * (_selectedBall->x - (float)GetMouseX());
				_selectedBall->vy = 20.0f * (_selectedBall->y - (float)GetMouseY());
			}
			_selectedBall = nullptr;
		}
	}

	void _updatePositionByVelocity(float& fElapsedTime) {
#pragma omp parallel for
		for (int i = 0; i < _balls.size(); i++)
		{
			if (_balls[i].remainingTime > 0) {
				// start position
				_balls[i].sx = _balls[i].x;
				_balls[i].sy = _balls[i].y;

				// Add Drag to emulate rolling friction
				_balls[i].ax = -_balls[i].vx * 0.8f;
				_balls[i].ay = -_balls[i].vy * 0.8f + _gravity;

				// Update ball physics
				_balls[i].vx += _balls[i].ax * _balls[i].remainingTime;
				_balls[i].vy += _balls[i].ay * _balls[i].remainingTime;
				_balls[i].x += _balls[i].vx * _balls[i].remainingTime;
				_balls[i].y += _balls[i].vy * _balls[i].remainingTime;

				// Wrap the balls around screen
				if (_balls[i].x < 0) _balls[i].x += (float)ScreenWidth();
				if (_balls[i].x >= ScreenWidth()) _balls[i].x -= (float)ScreenWidth();
				if (_balls[i].y < 0) _balls[i].y += (float)ScreenHeight();
				if (_balls[i].y >= ScreenHeight()) _balls[i].y -= (float)ScreenHeight();

				// Clamp velocity near zero
				if (fabs(_balls[i].vx * _balls[i].vx + _balls[i].vy * _balls[i].vy) < _minVelocity)
				{
					_balls[i].vx = 0;
					_balls[i].vy = 0;
				}
			}
		}
	}

	void _collision() {
		_staticCollision();
		_dynamicCollision();
	}

	void _staticCollision() {
		for (auto& ball : _balls) {
			// collide with line
			for (auto& line : _lines) {
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
			for (auto& target : _balls) {
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

	void _dynamicCollision() {
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

	void _updateBallColor() {
		for (auto& ball : _balls)
			ball.color;
	}

public:
	bool OnUserCreate() override
	{
		const int ball_num = 700;
		for (int i = 0; i < ball_num; i++) {
			_addBall(rand() % ScreenWidth(), rand() % ScreenHeight(), rand() % 12 + 5);
		}

		//_addLine(30.0f, 30.0f, 30.0f, 920.0f, 4.0f);
		//_addLine(1000.0f, 30.0f, 1000.0f, 920.0f, 4.0f);
		_addLine(30.0f, 920.0f, 1000.0f, 920.0f, 4.0f);
		_collisionList.reserve(ball_num * ball_num);
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		_selectAndDragBall();
		// Break up the frame elapsed time into smaller deltas for each simulation update
		float simElapsedTime = fElapsedTime / _timeSplit;
		for (int i = 0; i < _timeSplit; i++)
		{
			// Set all balls time to maximum for this epoch
			for (auto& ball : _balls)
				ball.remainingTime = simElapsedTime;

			_updatePositionByVelocity(fElapsedTime);
			_collision();
		}
		// clear the screen
		FillRect(0, 0, ScreenWidth(), ScreenHeight(), olc::Pixel(0, 0, 0));

		// draw ball
#pragma omp parallel for 
		for (int i = 0; i < _balls.size(); i++) {
			int r = std::min(sqrt(_balls[i].vx * _balls[i].vx + _balls[i].vy * _balls[i].vy) + 100.0f, 255.0f);
			FillCircle(_balls[i].x, _balls[i].y, _balls[i].r, olc::Pixel(r, 255 - r, 255 - r));
		}

		// draw line
		for (auto& line : _lines) {
			float nx = -(line.ey - line.sy);
			float ny = (line.ex - line.sx);
			float d = sqrt(nx * nx + ny * ny);
			nx /= d;
			ny /= d;
			DrawLine((line.sx + nx * line.r), (line.sy + ny * line.r), (line.ex + nx * line.r), (line.ey + ny * line.r), olc::GREY);
			DrawLine((line.sx - nx * line.r), (line.sy - ny * line.r), (line.ex - nx * line.r), (line.ey - ny * line.r), olc::GREY);
			FillCircle(line.sx, line.sy, line.r, olc::GREY);
			FillCircle(line.ex, line.ey, line.r, olc::GREY);
		}

		// draw right click line
		if (_selectedBall) {
			DrawLine(_selectedBall->x, _selectedBall->y, GetMouseX(), GetMouseY(), olc::BLUE);
		}
		return true;
	}
};


void RunBallCollision() {
	BallCollision ball_collision;
	if (ball_collision.Construct(1536, 1024, 1, 1))
		ball_collision.Start();
}