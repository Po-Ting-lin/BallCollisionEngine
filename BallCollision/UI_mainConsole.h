#pragma once
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "AppConfig.h"
#include "BallCollisionEngine.h"

#include <chrono>
#include <iomanip>
#include <sstream>

class BallCollision : public olc::PixelGameEngine
{
public:
	explicit BallCollision(const AppConfig& config)
		: _config(config),
		  _engine(nullptr),
		  _selectedBall(nullptr),
		  _selectedLine(nullptr),
		  _isStartLine(false),
		  _smoothedFrameSeconds(0.0f),
		  _hasLastFramePhases(false),
		  _hasSmoothedFramePhases(false) {
		sAppName = "BallCollision";
	}

	~BallCollision() override {
		delete _engine;
	}
private:
	AppConfig _config;
	BallCollisionEngine* _engine;
	Ball* _selectedBall;
	Line* _selectedLine;
	bool _isStartLine;
	float _smoothedFrameSeconds;

	struct FramePhases {
		double physicsSeconds = 0.0;
		double screenClearSeconds = 0.0;
		double ballDrawingSeconds = 0.0;
		double uiDrawingSeconds = 0.0;
		double presentationSeconds = 0.0;

		double measuredUserSeconds() const {
			return physicsSeconds
				+ screenClearSeconds
				+ ballDrawingSeconds
				+ uiDrawingSeconds;
		}
	};

	FramePhases _lastFramePhases;
	FramePhases _smoothedFramePhases;
	bool _hasLastFramePhases;
	bool _hasSmoothedFramePhases;

	inline std::string _formatMilliseconds(double seconds) const {
		std::ostringstream text;
		text << std::fixed << std::setprecision(3) << seconds * 1000.0;
		return text.str();
	}

	inline const char* _gravityName() const {
		switch (_engine->GetGravity()) {
		case Gravity::None:
			return "none";
		case Gravity::Normal:
			return "normal";
		case Gravity::Center:
			return "center";
		case Gravity::Bicenter:
			return "bicenter";
		}
		return "unknown";
	}

	inline void _updateFramePhaseAverages(
		float elapsedTime,
		float smoothing
	) {
		if (!_hasLastFramePhases)
			return;

		_lastFramePhases.presentationSeconds = std::max(
			0.0,
			static_cast<double>(elapsedTime)
				- _lastFramePhases.measuredUserSeconds()
		);

		if (!_hasSmoothedFramePhases) {
			_smoothedFramePhases = _lastFramePhases;
			_hasSmoothedFramePhases = true;
			return;
		}

		auto smooth = [smoothing](double& average, double sample) {
			average += (sample - average) * smoothing;
		};
		smooth(
			_smoothedFramePhases.physicsSeconds,
			_lastFramePhases.physicsSeconds
		);
		smooth(
			_smoothedFramePhases.screenClearSeconds,
			_lastFramePhases.screenClearSeconds
		);
		smooth(
			_smoothedFramePhases.ballDrawingSeconds,
			_lastFramePhases.ballDrawingSeconds
		);
		smooth(
			_smoothedFramePhases.uiDrawingSeconds,
			_lastFramePhases.uiDrawingSeconds
		);
		smooth(
			_smoothedFramePhases.presentationSeconds,
			_lastFramePhases.presentationSeconds
		);
	}

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
				_selectedBall->vx = _config.rightDragLaunchSpeed
					* (_selectedBall->x - static_cast<float>(GetMouseX()));
				_selectedBall->vy = _config.rightDragLaunchSpeed
					* (_selectedBall->y - static_cast<float>(GetMouseY()));
			}
			_selectedBall = nullptr;
		}
	}

public:
	bool OnUserCreate() override
	{
		_engine = new BallCollisionEngine(
			ScreenWidth(),
			ScreenHeight(),
			_config.numBalls,
			_config.minimumBallRadius,
			_config.maximumBallRadius,
			_config.gravity,
			_config.gravityStrength,
			_config.velocityDrag,
			_config.spawnPattern
		);
		_engine->Create();
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		using Clock = std::chrono::steady_clock;

		if (GetKey(olc::Key::ESCAPE).bPressed)
			return false;
		if (GetKey(olc::Key::G).bPressed)
			_engine->CycleGravity();

		if (fElapsedTime > 0.0f) {
			const float smoothing = std::min(1.0f, fElapsedTime * 5.0f);
			if (_smoothedFrameSeconds == 0.0f) {
				_smoothedFrameSeconds = fElapsedTime;
			}
			else {
				_smoothedFrameSeconds +=
					(fElapsedTime - _smoothedFrameSeconds) * smoothing;
			}
			_updateFramePhaseAverages(fElapsedTime, smoothing);
		}

		FramePhases currentFramePhases;
		_selectAndDragBall();

		auto phaseStart = Clock::now();
		_engine->Update(fElapsedTime);
		currentFramePhases.physicsSeconds =
			std::chrono::duration<double>(Clock::now() - phaseStart).count();

		// clear the screen
		phaseStart = Clock::now();
		FillRect(0, 0, ScreenWidth(), ScreenHeight(), olc::Pixel(0, 0, 0));
		currentFramePhases.screenClearSeconds =
			std::chrono::duration<double>(Clock::now() - phaseStart).count();

		// draw ball
		phaseStart = Clock::now();
		const int ballCount = static_cast<int>(_engine->Balls.size());
		for (int i = 0; i < ballCount; i++) {
			int r = std::min(sqrt(_engine->Balls[i].vx * _engine->Balls[i].vx + _engine->Balls[i].vy * _engine->Balls[i].vy) + 100.0f, 255.0f);
			FillCircle(_engine->Balls[i].x, _engine->Balls[i].y, _engine->Balls[i].r, olc::Pixel(r, 255 - r, 255 - r));
		}
		currentFramePhases.ballDrawingSeconds =
			std::chrono::duration<double>(Clock::now() - phaseStart).count();

		phaseStart = Clock::now();
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

		const int framesPerSecond = _smoothedFrameSeconds > 0.0f
			? static_cast<int>(1.0f / _smoothedFrameSeconds + 0.5f)
			: 0;
		const std::string fpsText = "FPS: " + std::to_string(framesPerSecond);
		FillRect(6, 6, 250, 114, olc::VERY_DARK_GREY);
		DrawString(10, 10, fpsText, olc::YELLOW, 2);
		DrawString(
			10,
			32,
			"Frame:     "
				+ _formatMilliseconds(_smoothedFrameSeconds) + " ms"
		);
		DrawString(
			10,
			44,
			"Physics:   "
				+ _formatMilliseconds(
					_smoothedFramePhases.physicsSeconds
				) + " ms"
		);
		DrawString(
			10,
			56,
			"Clear:     "
				+ _formatMilliseconds(
					_smoothedFramePhases.screenClearSeconds
				) + " ms"
		);
		DrawString(
			10,
			68,
			"Balls:     "
				+ _formatMilliseconds(
					_smoothedFramePhases.ballDrawingSeconds
				) + " ms"
		);
		DrawString(
			10,
			80,
			"UI:        "
				+ _formatMilliseconds(
					_smoothedFramePhases.uiDrawingSeconds
				) + " ms"
		);
		DrawString(
			10,
			92,
			"Present+:  "
				+ _formatMilliseconds(
					_smoothedFramePhases.presentationSeconds
				) + " ms"
		);
		DrawString(10, 104, "Gravity:   " + std::string(_gravityName()));

		currentFramePhases.uiDrawingSeconds =
			std::chrono::duration<double>(Clock::now() - phaseStart).count();
		_lastFramePhases = currentFramePhases;
		_hasLastFramePhases = true;

		return true;
	}
};


void RunBallCollision(const AppConfig& config) {
	BallCollision ball_collision(config);
	if (ball_collision.Construct(1920, 1080, 1, 1, true))
		ball_collision.Start();
}
