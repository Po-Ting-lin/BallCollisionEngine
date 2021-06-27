#pragma once
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "BallCollisionEngine.h"


class BallCollision : public olc::PixelGameEngine
{
public:
	BallCollision() { 
		sAppName = "BallCollision";
	}
private:
	BallCollisionEngine* _engine;
	Ball* _selectedBall;
	Line* _selectedLine;
	bool _isStartLine;

	inline bool _isPointInBall(float x1, float y1, float r1, float x, float y) {
		return fabs((x1 - x) * (x1 - x) + (y1 - y) * (y1 - y)) < (r1 * r1);
	}

	inline void _selectAndDragBall() {
		if (GetMouse(olc::Mouse::LEFT).bPressed || GetMouse(olc::Mouse::RIGHT).bPressed) {
			_selectedBall = nullptr;
			for (auto& ball : _engine->Balls) {
				if (_isPointInBall(ball.x, ball.y, ball.r, GetMouseX(), GetMouseY())) {
					_selectedBall = &ball; 
					break;
				}
			}

			_selectedLine = nullptr;
			for (auto& line : _engine->Lines) {
				if (_isPointInBall(line.sx, line.sy, line.r, GetMouseX(), GetMouseY())) {
					_selectedLine = &line;
					_isStartLine = true;
				}
			}
			for (auto& line : _engine->Lines) {
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

public:
	bool OnUserCreate() override
	{
		_engine = new BallCollisionEngine(ScreenWidth(), ScreenHeight(), 700);
		_engine->Create();
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		_selectAndDragBall();
		_engine->Update(fElapsedTime);

		// clear the screen
		FillRect(0, 0, ScreenWidth(), ScreenHeight(), olc::Pixel(0, 0, 0));

		// draw ball
#pragma omp parallel for 
		for (int i = 0; i < _engine->Balls.size(); i++) {
			int r = std::min(sqrt(_engine->Balls[i].vx * _engine->Balls[i].vx + _engine->Balls[i].vy * _engine->Balls[i].vy) + 100.0f, 255.0f);
			FillCircle(_engine->Balls[i].x, _engine->Balls[i].y, _engine->Balls[i].r, olc::Pixel(r, 255 - r, 255 - r));
		}

		// draw line
		for (auto& line : _engine->Lines) {
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