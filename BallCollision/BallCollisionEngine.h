#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <omp.h>
#include "Gravity.h"
#include "SpawnPattern.h"
#include "utils.h"
#include "objectStructure.h"

struct PhysicsMetrics {
	double integrationSeconds = 0.0;
	double gridBuildSeconds = 0.0;
	double collisionDetectionSeconds = 0.0;
	double collisionResponseSeconds = 0.0;
	std::uint64_t integrationUpdates = 0;
	std::uint64_t ballPairChecks = 0;
	std::uint64_t possibleBallPairs = 0;
	std::uint64_t lineChecks = 0;
	std::uint64_t collisionPairs = 0;
	std::uint64_t impulsesApplied = 0;
};

class BallCollisionEngine {
public:
	BallCollisionEngine(
		int width,
		int height,
		int ballNum,
		int minimumBallRadius = 2,
		int maximumBallRadius = 5,
		Gravity gravity = Gravity::Normal,
		float gravityStrength = 1.2f,
		float velocityDrag = 0.8f,
		SpawnPattern spawnPattern = SpawnPattern::Random
	);
	~BallCollisionEngine();

	void Create();
	void Update(float& fElapsedTime);
	void Update(float& fElapsedTime, PhysicsMetrics& metrics);
	void CycleGravity();
	Gravity GetGravity() const;
	void Reset();
	void AddLine(float sx, float sy, float ex, float ey, float radius);
	bool RemoveLineAt(float x, float y, float tolerance);
	void ClearLines();
	void SetMouseForce(float x, float y, float strength, float radius);
	void ClearMouseForce();

	std::vector<Ball> Balls;
	std::vector<Line> Lines;
private:
	// The fields the narrow phase reads and writes, copied out of Balls in
	// packed grid-cell order so each cell's balls are contiguous in memory.
	// Results are copied back to Balls at the end of every substep.
	struct CellBall {
		float x, y;
		float vx, vy;
		float r;
		float mass;
		std::uint32_t index; // position in Balls
	};

	std::vector<CellBall> _cellBalls;
	// Contacts found and impulses applied in the current substep, for metrics.
	std::uint64_t _substepContacts;
	std::uint64_t _substepImpulses;
	std::vector<std::size_t> _gridCellCounts;
	std::vector<std::size_t> _gridCellOffsets;
	std::vector<std::size_t> _gridBallCells;
	std::vector<std::size_t> _gridBallRows;
	std::vector<std::size_t> _gridSortScratch;
	std::vector<std::size_t> _gridColumnStarts;
	std::vector<std::size_t> _gridRowStarts;
	std::vector<std::size_t> _gridBallIndices;
	std::vector<std::size_t> _activeGridCells;
	int _ballNum;
	int _screenWidth;
	int _screenHeight;
	int _gridColumns;
	int _gridRows;
	int _minimumBallRadius;
	int _maximumBallRadius;
	float _timeSplit;
	float _minVelocity;
	float _gridCellSize;
	float _gravityStrength;
	float _velocityDrag;
	float _mouseForceX;
	float _mouseForceY;
	float _mouseForceStrength;
	float _mouseForceRadius;
	Gravity _gravity;
	SpawnPattern _spawnPattern;

	inline bool _isCollision(float x1, float y1, float r1, float x2, float y2, float r2) {
		return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)
			<= (r1 + r2) * (r1 + r2);
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

	void _update(float& fElapsedTime, PhysicsMetrics* metrics);
	void _createRandomPattern();
	void _createCircleClusterPattern();
	void _createRectangleClusterPattern();
	void _updatePositionByVelocity(float deltaTime);
	void _resolveBoundaryCollisions();
	void _staticCollision(PhysicsMetrics* metrics);
	void _resolveLineCollisions();
	void _buildSpatialGrid();
	void _gatherCellBalls();
	void _scatterCellBalls();
	std::uint64_t _resolveGridCollisions();
	void _resolveBallPair(std::size_t firstSlot, std::size_t secondSlot);

	// Used for both Ball and CellBall.
	template <typename BallType>
	void _resolveBoundaryCollision(BallType& ball) const {
		const float minimumX = ball.r;
		const float maximumX = static_cast<float>(_screenWidth) - ball.r;
		const float minimumY = ball.r;
		const float maximumY = static_cast<float>(_screenHeight) - ball.r;

		if (minimumX >= maximumX) {
			ball.x = _screenWidth * 0.5f;
			ball.vx = 0.0f;
		}
		else if (ball.x <= minimumX) {
			ball.x = minimumX;
			if (ball.vx < 0.0f)
				ball.vx = -ball.vx;
		}
		else if (ball.x >= maximumX) {
			ball.x = maximumX;
			if (ball.vx > 0.0f)
				ball.vx = -ball.vx;
		}

		if (minimumY >= maximumY) {
			ball.y = _screenHeight * 0.5f;
			ball.vy = 0.0f;
		}
		else if (ball.y <= minimumY) {
			ball.y = minimumY;
			if (ball.vy < 0.0f)
				ball.vy = -ball.vy;
		}
		else if (ball.y >= maximumY) {
			ball.y = maximumY;
			if (ball.vy > 0.0f)
				ball.vy = -ball.vy;
		}
	}
};

