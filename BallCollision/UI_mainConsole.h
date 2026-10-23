#pragma once
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "AppConfig.h"
#include "BallCollisionEngine.h"

#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

class BallCollision : public olc::PixelGameEngine
{
public:
	explicit BallCollision(const AppConfig& config)
		: _config(config),
		  _engine(nullptr),
		  _selectedBall(nullptr),
		  _selectedLine(nullptr),
		  _isStartLine(false),
		  _isDrawingWall(false),
		  _wallStartX(0.0f),
		  _wallStartY(0.0f),
		  _isPaused(false),
		  _timeScale(1.0f),
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
	bool _isDrawingWall;
	float _wallStartX;
	float _wallStartY;
	bool _isPaused;
	float _timeScale;
	float _smoothedFrameSeconds;

	static constexpr float MinimumTimeScale = 1.0f / 16.0f;
	static constexpr float MaximumTimeScale = 2.0f;

	struct FramePhases {
		double physicsSeconds = 0.0;
		double ballDrawingSeconds = 0.0;
		double uiDrawingSeconds = 0.0;
		double presentationSeconds = 0.0;

		double measuredUserSeconds() const {
			return physicsSeconds
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

	// Everything on screen is drawn by _renderScene() through a custom render
	// function on layer 0, so the 1920x1080 CPU framebuffer is never uploaded.
	struct GpuVertex {
		float x, y;
		float u, v;
		olc::Pixel color;
	};

	static constexpr int CircleTextureSize = 64;
	static constexpr int MouseForceSegments = 64;
	// The disc centre is fully opaque, so solid shapes and lines can share
	// the circle texture and be drawn in the same batch as the balls.
	static constexpr float SolidU = 0.5f;
	static constexpr float SolidV = 0.5f;

	std::vector<GpuVertex> _quadVertices;
	std::vector<GpuVertex> _lineVertices;
	GLuint _circleTexture = 0;
	std::unique_ptr<olc::Sprite> _hudSprite;
	std::unique_ptr<olc::Decal> _hudDecal;
	std::unique_ptr<olc::Sprite> _helpSprite;
	std::unique_ptr<olc::Decal> _helpDecal;
	int _helpY = 0;

	static constexpr int HudX = 6;
	static constexpr int HudY = 6;

	// White disc with anti-aliased edges and a hand-built mip chain, so balls
	// a few pixels wide still look round when the texture is minified.
	inline void _createCircleTexture() {
		constexpr int Samples = 4;

		glGenTextures(1, &_circleTexture);
		glBindTexture(GL_TEXTURE_2D, _circleTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		std::vector<olc::Pixel> texels;
		int level = 0;
		for (int size = CircleTextureSize; size >= 1; size /= 2, ++level) {
			const float radius = size * 0.5f;
			texels.assign(static_cast<std::size_t>(size) * size, olc::BLANK);
			for (int y = 0; y < size; ++y) {
				for (int x = 0; x < size; ++x) {
					int covered = 0;
					for (int sy = 0; sy < Samples; ++sy) {
						for (int sx = 0; sx < Samples; ++sx) {
							const float px = x + (sx + 0.5f) / Samples - radius;
							const float py = y + (sy + 0.5f) / Samples - radius;
							if (px * px + py * py <= radius * radius)
								++covered;
						}
					}
					const int alpha = covered * 255 / (Samples * Samples);
					texels[static_cast<std::size_t>(y) * size + x] =
						olc::Pixel(255, 255, 255, static_cast<uint8_t>(alpha));
				}
			}
			glTexImage2D(
				GL_TEXTURE_2D,
				level,
				GL_RGBA,
				size,
				size,
				0,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				texels.data()
			);
		}
	}

	inline void _pushCircle(float x, float y, float radius, olc::Pixel color) {
		_quadVertices.push_back({ x - radius, y - radius, 0.0f, 0.0f, color });
		_quadVertices.push_back({ x + radius, y - radius, 1.0f, 0.0f, color });
		_quadVertices.push_back({ x + radius, y + radius, 1.0f, 1.0f, color });
		_quadVertices.push_back({ x - radius, y + radius, 0.0f, 1.0f, color });
	}

	inline void _pushSolidQuad(
		float x0, float y0,
		float x1, float y1,
		float x2, float y2,
		float x3, float y3,
		olc::Pixel color
	) {
		_quadVertices.push_back({ x0, y0, SolidU, SolidV, color });
		_quadVertices.push_back({ x1, y1, SolidU, SolidV, color });
		_quadVertices.push_back({ x2, y2, SolidU, SolidV, color });
		_quadVertices.push_back({ x3, y3, SolidU, SolidV, color });
	}

	inline void _pushLine(float x0, float y0, float x1, float y1, olc::Pixel color) {
		_lineVertices.push_back({ x0, y0, SolidU, SolidV, color });
		_lineVertices.push_back({ x1, y1, SolidU, SolidV, color });
	}

	inline void _pushCircleOutline(float x, float y, float radius, olc::Pixel color) {
		constexpr float TwoPi = 6.28318530718f;
		for (int i = 0; i < MouseForceSegments; ++i) {
			const float a0 = TwoPi * i / MouseForceSegments;
			const float a1 = TwoPi * (i + 1) / MouseForceSegments;
			_pushLine(
				x + radius * std::cos(a0), y + radius * std::sin(a0),
				x + radius * std::cos(a1), y + radius * std::sin(a1),
				color
			);
		}
	}

	inline void _drawVertices(GLenum mode, const GpuVertex* vertices, std::size_t count) {
		if (count == 0)
			return;
		glVertexPointer(2, GL_FLOAT, sizeof(GpuVertex), &vertices->x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(GpuVertex), &vertices->u);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GpuVertex), &vertices->color);
		glDrawArrays(mode, 0, static_cast<GLsizei>(count));
	}

	inline void _drawOverlay(olc::Decal& decal, float x, float y) {
		const float w = static_cast<float>(decal.sprite->width);
		const float h = static_cast<float>(decal.sprite->height);
		const GpuVertex quad[4] = {
			{ x, y, 0.0f, 0.0f, olc::WHITE },
			{ x + w, y, 1.0f, 0.0f, olc::WHITE },
			{ x + w, y + h, 1.0f, 1.0f, olc::WHITE },
			{ x, y + h, 0.0f, 1.0f, olc::WHITE }
		};
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(decal.id));
		_drawVertices(GL_QUADS, quad, 4);
	}

	// Called by the engine in place of drawing layer 0. Balls, walls and
	// lines go out in two draw calls; the HUD is two small textures.
	inline void _renderScene() {
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0, ScreenWidth(), ScreenHeight(), 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glBindTexture(GL_TEXTURE_2D, _circleTexture);
		_drawVertices(GL_QUADS, _quadVertices.data(), _quadVertices.size());
		_drawVertices(GL_LINES, _lineVertices.data(), _lineVertices.size());

		_drawOverlay(*_hudDecal, HudX, HudY);
		_drawOverlay(*_helpDecal, HudX, static_cast<float>(_helpY - 4));

		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
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

		// right press on empty space starts drawing a wall
		if (GetMouse(olc::Mouse::RIGHT).bPressed && !_selectedBall) {
			_isDrawingWall = true;
			_wallStartX = static_cast<float>(GetMouseX());
			_wallStartY = static_cast<float>(GetMouseY());
		}

		if (GetMouse(olc::Mouse::RIGHT).bReleased) {
			if (_selectedBall) {
				_selectedBall->vx = _config.rightDragLaunchSpeed
					* (_selectedBall->x - static_cast<float>(GetMouseX()));
				_selectedBall->vy = _config.rightDragLaunchSpeed
					* (_selectedBall->y - static_cast<float>(GetMouseY()));
			}
			_selectedBall = nullptr;

			if (_isDrawingWall) {
				const float dx = GetMouseX() - _wallStartX;
				const float dy = GetMouseY() - _wallStartY;
				const float minimumLength = _config.wallRadius * 2.0f;
				if (dx * dx + dy * dy >= minimumLength * minimumLength) {
					_engine->AddLine(
						_wallStartX,
						_wallStartY,
						static_cast<float>(GetMouseX()),
						static_cast<float>(GetMouseY()),
						_config.wallRadius
					);
					// Lines may have reallocated
					_selectedLine = nullptr;
				}
				_isDrawingWall = false;
			}
		}

		if (GetMouse(olc::Mouse::MIDDLE).bPressed) {
			if (_engine->RemoveLineAt(
				static_cast<float>(GetMouseX()),
				static_cast<float>(GetMouseY()),
				4.0f
			)) {
				_selectedLine = nullptr;
			}
		}
	}

	inline void _handleKeys() {
		if (GetKey(olc::Key::P).bPressed || GetKey(olc::Key::SPACE).bPressed)
			_isPaused = !_isPaused;
		if (GetKey(olc::Key::MINUS).bPressed)
			_timeScale = std::max(MinimumTimeScale, _timeScale * 0.5f);
		if (GetKey(olc::Key::EQUALS).bPressed)
			_timeScale = std::min(MaximumTimeScale, _timeScale * 2.0f);
		if (GetKey(olc::Key::K0).bPressed)
			_timeScale = 1.0f;
		if (GetKey(olc::Key::R).bPressed) {
			_selectedBall = nullptr;
			_engine->Reset();
		}
		if (GetKey(olc::Key::C).bPressed) {
			_selectedLine = nullptr;
			_isDrawingWall = false;
			_engine->ClearLines();
		}

		// hold Q to pull balls toward the cursor, E to push them away
		float forceDirection = 0.0f;
		if (GetKey(olc::Key::Q).bHeld)
			forceDirection += 1.0f;
		if (GetKey(olc::Key::E).bHeld)
			forceDirection -= 1.0f;
		if (forceDirection != 0.0f) {
			_engine->SetMouseForce(
				static_cast<float>(GetMouseX()),
				static_cast<float>(GetMouseY()),
				forceDirection * _config.mouseForceStrength,
				_config.mouseForceRadius
			);
		}
		else {
			_engine->ClearMouseForce();
		}
	}

	inline std::string _speedText() const {
		if (_isPaused)
			return "PAUSED";
		std::ostringstream text;
		text << std::fixed << std::setprecision(
			_timeScale < 0.25f ? 4 : 2
		) << _timeScale << "x";
		return text.str();
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

		const std::size_t expectedQuadVertices =
			static_cast<std::size_t>(_config.numBalls + 1) * 4;
		_quadVertices.reserve(expectedQuadVertices);
		_createCircleTexture();

		_hudSprite = std::make_unique<olc::Sprite>(250, 114);
		_hudDecal = std::make_unique<olc::Decal>(_hudSprite.get());

		_helpY = ScreenHeight() - 30;
		_helpSprite = std::make_unique<olc::Sprite>(620, 28);
		SetDrawTarget(_helpSprite.get());
		Clear(olc::VERY_DARK_GREY);
		DrawString(
			4,
			4,
			"LMB drag  RMB ball:launch  RMB empty:draw wall  MMB:delete wall"
		);
		DrawString(
			4,
			16,
			"Q/E pull/push  P pause  -/= speed  0 1x  R reset  C clear walls  G gravity"
		);
		SetDrawTarget(nullptr);
		_helpDecal = std::make_unique<olc::Decal>(_helpSprite.get());

		SetLayerCustomRenderFunction(0, [this]() { _renderScene(); });
		return true;
	}

	bool OnUserDestroy() override
	{
		// GL objects must be released while the context still exists
		_hudDecal.reset();
		_helpDecal.reset();
		if (_circleTexture != 0) {
			glDeleteTextures(1, &_circleTexture);
			_circleTexture = 0;
		}
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
		_handleKeys();
		_selectAndDragBall();

		auto phaseStart = Clock::now();
		if (!_isPaused) {
			float simulationElapsedTime = fElapsedTime * _timeScale;
			_engine->Update(simulationElapsedTime);
		}
		currentFramePhases.physicsSeconds =
			std::chrono::duration<double>(Clock::now() - phaseStart).count();

		// build ball geometry for the GPU
		phaseStart = Clock::now();
		_quadVertices.clear();
		_lineVertices.clear();
		for (const Ball& ball : _engine->Balls) {
			int r = std::min(std::sqrt(ball.vx * ball.vx + ball.vy * ball.vy) + 100.0f, 255.0f);
			_pushCircle(ball.x, ball.y, ball.r, olc::Pixel(r, 255 - r, 255 - r));
		}
		currentFramePhases.ballDrawingSeconds =
			std::chrono::duration<double>(Clock::now() - phaseStart).count();

		phaseStart = Clock::now();
		// draw walls as capsules
		for (auto& line : _engine->Lines) {
			float nx = -(line.ey - line.sy);
			float ny = (line.ex - line.sx);
			float d = std::sqrt(nx * nx + ny * ny);
			if (d > 0.0f) {
				nx = nx / d * line.r;
				ny = ny / d * line.r;
				_pushSolidQuad(
					line.sx + nx, line.sy + ny,
					line.ex + nx, line.ey + ny,
					line.ex - nx, line.ey - ny,
					line.sx - nx, line.sy - ny,
					olc::GREY
				);
			}
			_pushCircle(line.sx, line.sy, line.r, olc::GREY);
			_pushCircle(line.ex, line.ey, line.r, olc::GREY);
		}

		const float mouseX = static_cast<float>(GetMouseX());
		const float mouseY = static_cast<float>(GetMouseY());

		// draw right click line
		if (_selectedBall) {
			_pushLine(_selectedBall->x, _selectedBall->y, mouseX, mouseY, olc::BLUE);
		}

		// draw wall preview
		if (_isDrawingWall) {
			_pushLine(_wallStartX, _wallStartY, mouseX, mouseY, olc::WHITE);
			_pushCircle(_wallStartX, _wallStartY, _config.wallRadius, olc::WHITE);
		}

		// draw mouse force radius
		if (GetKey(olc::Key::Q).bHeld != GetKey(olc::Key::E).bHeld) {
			_pushCircleOutline(
				mouseX,
				mouseY,
				_config.mouseForceRadius,
				GetKey(olc::Key::Q).bHeld ? olc::GREEN : olc::RED
			);
		}

		const int framesPerSecond = _smoothedFrameSeconds > 0.0f
			? static_cast<int>(1.0f / _smoothedFrameSeconds + 0.5f)
			: 0;
		const std::string fpsText = "FPS: " + std::to_string(framesPerSecond);
		SetDrawTarget(_hudSprite.get());
		Clear(olc::VERY_DARK_GREY);
		DrawString(4, 4, fpsText, olc::YELLOW, 2);
		DrawString(
			4,
			26,
			"Frame:     "
				+ _formatMilliseconds(_smoothedFrameSeconds) + " ms"
		);
		DrawString(
			4,
			38,
			"Physics:   "
				+ _formatMilliseconds(
					_smoothedFramePhases.physicsSeconds
				) + " ms"
		);
		DrawString(
			4,
			50,
			"Balls:     "
				+ _formatMilliseconds(
					_smoothedFramePhases.ballDrawingSeconds
				) + " ms"
		);
		DrawString(
			4,
			62,
			"UI:        "
				+ _formatMilliseconds(
					_smoothedFramePhases.uiDrawingSeconds
				) + " ms"
		);
		DrawString(
			4,
			74,
			"Present+:  "
				+ _formatMilliseconds(
					_smoothedFramePhases.presentationSeconds
				) + " ms"
		);
		DrawString(4, 86, "Gravity:   " + std::string(_gravityName()));
		DrawString(
			4,
			98,
			"Speed:     " + _speedText(),
			_isPaused ? olc::YELLOW : olc::WHITE
		);
		SetDrawTarget(nullptr);
		_hudDecal->Update();

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
