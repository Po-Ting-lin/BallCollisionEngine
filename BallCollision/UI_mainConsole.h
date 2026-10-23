#pragma once
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "AllocationCounter.h"
#include "AppConfig.h"
#include "BallCollisionEngine.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
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

	// Reused for every HUD line; assign() keeps the capacity reserved in
	// OnUserCreate, so formatting does not touch the heap.
	std::string _hudText;

	template <typename... Args>
	inline const std::string& _formatHud(const char* format, Args... args) {
		char buffer[64];
		std::snprintf(buffer, sizeof(buffer), format, args...);
		_hudText.assign(buffer);
		return _hudText;
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

	// --frame-benchmark runs these scenes back to back with a fixed
	// simulation step and records every phase of every frame.
	struct FrameBenchmarkScene {
		std::string name;
		int numBalls;
		int minimumBallRadius;
		int maximumBallRadius;
		SpawnPattern spawnPattern;
		Gravity gravity;
		float gravityStrength;
		float velocityDrag;
		bool addWalls = false;
	};

	struct FrameSample {
		double physicsSeconds = 0.0;
		double ballDrawingSeconds = 0.0;
		double uiDrawingSeconds = 0.0;
		double renderSeconds = 0.0;
		double presentationSeconds = 0.0;
		double frameSeconds = 0.0;
		std::uint64_t allocations = 0;
	};

	struct PhaseStatistics {
		double average = 0.0;
		double median = 0.0;
		double p95 = 0.0;
		double p99 = 0.0;
		double maximum = 0.0;
	};

	struct SceneResult {
		std::string name;
		std::size_t ballCount = 0;
		double averageFps = 0.0;
		PhaseStatistics phases[6];
		PhaseStatistics allocations;
	};

	static constexpr int BenchmarkWarmupFrames = 60;
	static constexpr std::size_t BenchmarkMeasuredFrames = 600;
	static constexpr float BenchmarkFrameTime = 1.0f / 60.0f;
	static constexpr const char* PhaseNames[6] = {
		"Physics", "Balls", "UI", "Render (GPU)", "Present+", "Frame"
	};
	static constexpr const char* PhaseColumns[6] = {
		"physics", "balls", "ui", "render", "present", "frame"
	};

	std::vector<FrameBenchmarkScene> _benchmarkScenes;
	std::vector<FrameSample> _benchmarkSamples;
	std::vector<SceneResult> _benchmarkResults;
	std::size_t _benchmarkSceneIndex = 0;
	int _benchmarkFrame = 0;
	bool _hasPendingBenchmarkSample = false;
	FrameSample _pendingBenchmarkSample;
	double _lastRenderSeconds = 0.0;
	std::uint64_t _frameStartAllocationCount = 0;

	inline void _createEngine(
		int numBalls,
		int minimumBallRadius,
		int maximumBallRadius,
		Gravity gravity,
		float gravityStrength,
		float velocityDrag,
		SpawnPattern spawnPattern
	) {
		delete _engine;
		_engine = new BallCollisionEngine(
			ScreenWidth(),
			ScreenHeight(),
			numBalls,
			minimumBallRadius,
			maximumBallRadius,
			gravity,
			gravityStrength,
			velocityDrag,
			spawnPattern
		);
		_engine->Create();
		_selectedBall = nullptr;
		_selectedLine = nullptr;
		_isDrawingWall = false;

		_reserveVertices(numBalls);
	}

	// clear() keeps capacity, so reserving once keeps the per-frame geometry
	// build off the heap. Walls take 12 quad vertices each and come out of the
	// headroom.
	inline void _reserveVertices(int numBalls) {
		constexpr std::size_t WallVertexHeadroom = 4'096;
		_quadVertices.reserve(
			static_cast<std::size_t>(numBalls + 1) * 4 + WallVertexHeadroom
		);
		_lineVertices.reserve(MouseForceSegments * 2 + 64);
	}

	inline static const char* _spawnPatternName(SpawnPattern pattern) {
		switch (pattern) {
		case SpawnPattern::Random:
			return "random";
		case SpawnPattern::CircleCluster:
			return "circle_cluster";
		case SpawnPattern::RectangleCluster:
			return "rectangle_cluster";
		}
		return "unknown";
	}

	inline void _startBenchmarkScene() {
		const FrameBenchmarkScene& scene = _benchmarkScenes[_benchmarkSceneIndex];
		std::srand(0xC0FFEEu + static_cast<unsigned int>(_benchmarkSceneIndex));
		_createEngine(
			scene.numBalls,
			scene.minimumBallRadius,
			scene.maximumBallRadius,
			scene.gravity,
			scene.gravityStrength,
			scene.velocityDrag,
			scene.spawnPattern
		);
		if (scene.addWalls) {
			// an octagon of walls around the gravity centre
			constexpr int WallCount = 8;
			constexpr float TwoPi = 6.28318530718f;
			const float centerX = ScreenWidth() * 0.5f;
			const float centerY = ScreenHeight() * 0.5f;
			const float wallDistance = ScreenHeight() * 0.3f;
			for (int wall = 0; wall < WallCount; ++wall) {
				const float a0 = TwoPi * wall / WallCount;
				const float a1 = TwoPi * (wall + 1) / WallCount;
				_engine->AddLine(
					centerX + wallDistance * std::cos(a0),
					centerY + wallDistance * std::sin(a0),
					centerX + wallDistance * std::cos(a1),
					centerY + wallDistance * std::sin(a1),
					_config.wallRadius
				);
			}
		}
		_benchmarkFrame = 0;
		_benchmarkSamples.clear();
		_benchmarkSamples.reserve(BenchmarkMeasuredFrames);
		_hasPendingBenchmarkSample = false;
	}

	inline static PhaseStatistics _computeStatistics(std::vector<double> values) {
		PhaseStatistics statistics;
		if (values.empty())
			return statistics;

		std::sort(values.begin(), values.end());
		double sum = 0.0;
		for (double value : values)
			sum += value;

		// nearest-rank percentile
		auto percentile = [&values](double fraction) {
			const std::size_t rank = static_cast<std::size_t>(
				std::ceil(fraction * values.size())
			);
			return values[std::clamp<std::size_t>(rank, 1, values.size()) - 1];
		};
		statistics.average = sum / values.size();
		statistics.median = percentile(0.5);
		statistics.p95 = percentile(0.95);
		statistics.p99 = percentile(0.99);
		statistics.maximum = values.back();
		return statistics;
	}

	inline void _reportBenchmarkScene() {
		const FrameBenchmarkScene& scene = _benchmarkScenes[_benchmarkSceneIndex];
		SceneResult result;
		result.name = scene.name;
		result.ballCount = _engine->Balls.size();

		std::vector<double> values(_benchmarkSamples.size());
		for (int phase = 0; phase < 6; ++phase) {
			for (std::size_t index = 0; index < _benchmarkSamples.size(); ++index) {
				const FrameSample& sample = _benchmarkSamples[index];
				const double phaseSeconds[6] = {
					sample.physicsSeconds,
					sample.ballDrawingSeconds,
					sample.uiDrawingSeconds,
					sample.renderSeconds,
					sample.presentationSeconds,
					sample.frameSeconds
				};
				values[index] = phaseSeconds[phase] * 1000.0;
			}
			result.phases[phase] = _computeStatistics(values);
		}
		for (std::size_t index = 0; index < _benchmarkSamples.size(); ++index)
			values[index] = static_cast<double>(_benchmarkSamples[index].allocations);
		result.allocations = _computeStatistics(values);
		result.averageFps = result.phases[5].average > 0.0
			? 1000.0 / result.phases[5].average
			: 0.0;

		std::cout << std::fixed << std::setprecision(3)
			<< "\n### Scene " << _benchmarkSceneIndex + 1
			<< "/" << _benchmarkScenes.size() << ": " << scene.name
			<< " (" << result.ballCount << " balls, radius "
			<< scene.minimumBallRadius << "-" << scene.maximumBallRadius
			<< ", " << _spawnPatternName(scene.spawnPattern)
			<< ", gravity " << _gravityName() << ")\n\n"
			<< "| Phase | Avg ms | Median ms | P95 ms | P99 ms | Max ms |\n"
			<< "|:---|---:|---:|---:|---:|---:|\n";
		for (int phase = 0; phase < 6; ++phase) {
			const PhaseStatistics& statistics = result.phases[phase];
			std::cout << "| " << PhaseNames[phase]
				<< " | " << statistics.average
				<< " | " << statistics.median
				<< " | " << statistics.p95
				<< " | " << statistics.p99
				<< " | " << statistics.maximum << " |\n";
		}
		std::cout << "\nAverage FPS: " << result.averageFps
			<< "\nHeap allocations per frame: avg " << result.allocations.average
			<< ", p99 " << result.allocations.p99
			<< ", max " << result.allocations.maximum << "\n" << std::flush;

		if (!_config.frameBenchmarkOutput.empty())
			_appendBenchmarkCsv(result);
		_benchmarkResults.push_back(result);
	}

	inline void _appendBenchmarkCsv(const SceneResult& result) const {
		const std::filesystem::path& path = _config.frameBenchmarkOutput;
		std::error_code error;
		const bool writeHeader = !std::filesystem::exists(path, error)
			|| std::filesystem::file_size(path, error) == 0;

		std::ofstream output(path, std::ios::app);
		if (!output) {
			std::cerr << "Unable to write frame benchmark CSV: "
				<< path.string() << '\n';
			return;
		}

		if (writeHeader) {
			output << "scene,balls,frames,avg_fps";
			for (const char* column : PhaseColumns) {
				output << ',' << column << "_avg_ms"
					<< ',' << column << "_median_ms"
					<< ',' << column << "_p95_ms"
					<< ',' << column << "_p99_ms"
					<< ',' << column << "_max_ms";
			}
			output << ",allocs_avg,allocs_p99,allocs_max\n";
		}

		output << std::fixed << std::setprecision(4)
			<< '"' << result.name << '"'
			<< ',' << result.ballCount
			<< ',' << BenchmarkMeasuredFrames
			<< ',' << result.averageFps;
		for (const PhaseStatistics& statistics : result.phases) {
			output << ',' << statistics.average
				<< ',' << statistics.median
				<< ',' << statistics.p95
				<< ',' << statistics.p99
				<< ',' << statistics.maximum;
		}
		output << ',' << result.allocations.average
			<< ',' << result.allocations.p99
			<< ',' << result.allocations.maximum << '\n';
	}

	inline void _printBenchmarkSummary() const {
		std::cout << std::fixed << std::setprecision(3)
			<< "\n## Frame benchmark summary\n\n"
			<< "| Scene | Balls | Avg FPS | Frame avg ms | Frame p99 ms"
			<< " | Physics avg ms | Balls avg ms | UI avg ms"
			<< " | Render (GPU) avg ms | Render p99 ms | Present+ avg ms"
			<< " | Allocs/frame avg | Allocs/frame max |\n"
			<< "|:---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
		for (const SceneResult& result : _benchmarkResults) {
			std::cout << "| " << result.name
				<< " | " << result.ballCount
				<< " | " << result.averageFps
				<< " | " << result.phases[5].average
				<< " | " << result.phases[5].p99
				<< " | " << result.phases[0].average
				<< " | " << result.phases[1].average
				<< " | " << result.phases[2].average
				<< " | " << result.phases[3].average
				<< " | " << result.phases[3].p99
				<< " | " << result.phases[4].average
				<< " | " << result.allocations.average
				<< " | " << result.allocations.maximum << " |\n";
		}
		std::cout << std::flush;
	}

	// Called at the start of each frame. fElapsedTime covers the whole of the
	// previous frame, so the previous frame's sample is completed here.
	// Returns false once the last scene has been reported.
	inline bool _advanceFrameBenchmark(float elapsedTime) {
		const std::uint64_t allocationCount = AllocationCount();
		const std::uint64_t frameAllocations =
			allocationCount - _frameStartAllocationCount;
		_frameStartAllocationCount = allocationCount;

		if (_hasPendingBenchmarkSample) {
			FrameSample& sample = _pendingBenchmarkSample;
			sample.renderSeconds = _lastRenderSeconds;
			sample.frameSeconds = elapsedTime;
			sample.allocations = frameAllocations;
			sample.presentationSeconds = std::max(
				0.0,
				static_cast<double>(elapsedTime)
					- sample.physicsSeconds
					- sample.ballDrawingSeconds
					- sample.uiDrawingSeconds
					- sample.renderSeconds
			);
			_benchmarkSamples.push_back(sample);
			_hasPendingBenchmarkSample = false;
		}

		if (_benchmarkSamples.size() < BenchmarkMeasuredFrames)
			return true;

		_reportBenchmarkScene();
		++_benchmarkSceneIndex;
		if (_benchmarkSceneIndex >= _benchmarkScenes.size()) {
			_printBenchmarkSummary();
			return false;
		}
		_startBenchmarkScene();
		return true;
	}

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
		const auto renderStart = std::chrono::steady_clock::now();

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

		// Wait for the GPU so its draw time is separated from SwapBuffers.
		if (_config.frameBenchmark) {
			glFinish();
			_lastRenderSeconds = std::chrono::duration<double>(
				std::chrono::steady_clock::now() - renderStart
			).count();
		}
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

	inline const std::string& _formatSpeedText() {
		if (_config.frameBenchmark) {
			return _formatHud(
				"Speed:     BENCH %d/%d",
				static_cast<int>(_benchmarkSceneIndex + 1),
				static_cast<int>(_benchmarkScenes.size())
			);
		}
		if (_isPaused)
			return _formatHud("Speed:     %s", "PAUSED");
		return _formatHud(
			"Speed:     %.*fx",
			_timeScale < 0.25f ? 4 : 2,
			static_cast<double>(_timeScale)
		);
	}

	// Decal::Update() re-specifies the texture with glTexImage2D, which can
	// make the driver reallocate its storage every frame. The decal already
	// owns a texture of the right size, so only its pixels are replaced.
	inline void _uploadHudTexture() {
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(_hudDecal->id));
		glTexSubImage2D(
			GL_TEXTURE_2D,
			0,
			0,
			0,
			_hudSprite->width,
			_hudSprite->height,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			_hudSprite->GetData()
		);
	}

public:
	bool OnUserCreate() override
	{
		if (_config.frameBenchmark) {
			_benchmarkScenes = {
				{ "random 2k center", 2'000, 2, 3, SpawnPattern::Random, Gravity::Center, 1.2f, 0.8f },
				{ "random 8k center", 8'000, 2, 3, SpawnPattern::Random, Gravity::Center, 1.2f, 0.8f },
				{ "random 20k center", 20'000, 2, 3, SpawnPattern::Random, Gravity::Center, 1.2f, 0.8f },
				{ "random 8k center walls", 8'000, 2, 3, SpawnPattern::Random, Gravity::Center, 1.2f, 0.8f, true },
				{
					"config.ini",
					_config.numBalls,
					_config.minimumBallRadius,
					_config.maximumBallRadius,
					_config.spawnPattern,
					_config.gravity,
					_config.gravityStrength,
					_config.velocityDrag
				}
			};
			std::cout << "BallCollision frame benchmark: "
				<< ScreenWidth() << "x" << ScreenHeight() << ", "
				<< BenchmarkWarmupFrames << " warm-up + "
				<< BenchmarkMeasuredFrames << " measured frames per scene, "
				<< "fixed simulation step 1/60 s\n";
			if (const char* glRenderer = reinterpret_cast<const char*>(
				glGetString(GL_RENDERER))) {
				std::cout << "GL renderer: " << glRenderer << "\n";
			}
			int largestScene = 0;
			for (const FrameBenchmarkScene& scene : _benchmarkScenes)
				largestScene = std::max(largestScene, scene.numBalls);
			_reserveVertices(largestScene);
			_startBenchmarkScene();
		}
		else {
			_createEngine(
				_config.numBalls,
				_config.minimumBallRadius,
				_config.maximumBallRadius,
				_config.gravity,
				_config.gravityStrength,
				_config.velocityDrag,
				_config.spawnPattern
			);
		}
		_createCircleTexture();

		_hudText.reserve(64);
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
		if (_config.frameBenchmark && !_advanceFrameBenchmark(fElapsedTime))
			return false;
		if (!_config.frameBenchmark && GetKey(olc::Key::G).bPressed)
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
		if (!_config.frameBenchmark) {
			_handleKeys();
			_selectAndDragBall();
		}

		auto phaseStart = Clock::now();
		if (_config.frameBenchmark) {
			float simulationElapsedTime = BenchmarkFrameTime;
			_engine->Update(simulationElapsedTime);
		}
		else if (!_isPaused) {
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
		if (!_config.frameBenchmark
			&& GetKey(olc::Key::Q).bHeld != GetKey(olc::Key::E).bHeld) {
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
		// HUD text goes through one reused string so the frame does not allocate
		SetDrawTarget(_hudSprite.get());
		Clear(olc::VERY_DARK_GREY);
		DrawString(4, 4, _formatHud("FPS: %d", framesPerSecond), olc::YELLOW, 2);
		DrawString(
			4,
			26,
			_formatHud("Frame:     %.3f ms", _smoothedFrameSeconds * 1000.0)
		);
		DrawString(
			4,
			38,
			_formatHud(
				"Physics:   %.3f ms",
				_smoothedFramePhases.physicsSeconds * 1000.0
			)
		);
		DrawString(
			4,
			50,
			_formatHud(
				"Balls:     %.3f ms",
				_smoothedFramePhases.ballDrawingSeconds * 1000.0
			)
		);
		DrawString(
			4,
			62,
			_formatHud(
				"UI:        %.3f ms",
				_smoothedFramePhases.uiDrawingSeconds * 1000.0
			)
		);
		DrawString(
			4,
			74,
			_formatHud(
				"Present+:  %.3f ms",
				_smoothedFramePhases.presentationSeconds * 1000.0
			)
		);
		DrawString(4, 86, _formatHud("Gravity:   %s", _gravityName()));
		DrawString(
			4,
			98,
			_formatSpeedText(),
			_isPaused ? olc::YELLOW : olc::WHITE
		);
		SetDrawTarget(nullptr);
		_uploadHudTexture();

		currentFramePhases.uiDrawingSeconds =
			std::chrono::duration<double>(Clock::now() - phaseStart).count();
		_lastFramePhases = currentFramePhases;
		_hasLastFramePhases = true;

		if (_config.frameBenchmark) {
			if (_benchmarkFrame >= BenchmarkWarmupFrames) {
				_pendingBenchmarkSample = FrameSample();
				_pendingBenchmarkSample.physicsSeconds =
					currentFramePhases.physicsSeconds;
				_pendingBenchmarkSample.ballDrawingSeconds =
					currentFramePhases.ballDrawingSeconds;
				_pendingBenchmarkSample.uiDrawingSeconds =
					currentFramePhases.uiDrawingSeconds;
				_hasPendingBenchmarkSample = true;
			}
			++_benchmarkFrame;
		}

		return true;
	}
};


void RunBallCollision(const AppConfig& config) {
	BallCollision ball_collision(config);
	if (ball_collision.Construct(1920, 1080, 1, 1, true))
		ball_collision.Start();
}
