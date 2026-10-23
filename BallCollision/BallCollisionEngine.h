#pragma once
#include <cstdint>
#include <cmath>
#include <deque>
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
	std::deque<Ball> _fakeBalls;
	std::vector<std::pair<Ball*, Ball*>> _collisionList;
	std::vector<std::size_t> _gridCellCounts;
	std::vector<std::size_t> _gridCellOffsets;
	std::vector<std::size_t> _gridCellWriteOffsets;
	std::vector<std::size_t> _gridBallCells;
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
	void _updatePositionByVelocity(float& fElapsedTime);
	void _resolveBoundaryCollision(Ball& ball) const;
	void _resolveBoundaryCollisions();
	void _staticCollision(PhysicsMetrics* metrics);
	void _resolveLineCollisions();
	void _buildSpatialGrid();
	std::uint64_t _resolveGridCollisions();
	void _resolveBallPair(std::size_t firstIndex, std::size_t secondIndex);
	void _updateRemainingTimes();
	std::uint64_t _dynamicCollision();
};

