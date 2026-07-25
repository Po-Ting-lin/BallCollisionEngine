#include "BallCollisionEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <stdexcept>


namespace {
	struct ClusterPoint {
		float x;
		float y;
		float distanceSquared;
	};
}


BallCollisionEngine::BallCollisionEngine(
	int width,
	int height,
	int ballNum,
	int minimumBallRadius,
	int maximumBallRadius,
	Gravity gravity,
	float gravityStrength,
	float velocityDrag,
	SpawnPattern spawnPattern
) {
	_screenHeight = height;
	_screenWidth = width;
	_ballNum = ballNum;
	_gridColumns = 0;
	_gridRows = 0;
	_minimumBallRadius = minimumBallRadius;
	_maximumBallRadius = maximumBallRadius;
	_timeSplit = 4.0f;
	_minVelocity = 0.1f;
	_gridCellSize = 1.0f;
	_gravity = gravity;
	_gravityStrength = gravityStrength;
	_velocityDrag = velocityDrag;
	_spawnPattern = spawnPattern;
}

BallCollisionEngine::~BallCollisionEngine() {
	Balls.clear();
	Lines.clear();
	_fakeBalls.clear();
	_collisionList.clear();
	_gridCellCounts.clear();
	_gridCellOffsets.clear();
	_gridCellWriteOffsets.clear();
	_gridBallCells.clear();
	_gridBallIndices.clear();
	_activeGridCells.clear();
}


void BallCollisionEngine::Create() {
	Balls.reserve(Balls.size() + static_cast<std::size_t>(_ballNum));

	switch (_spawnPattern) {
	case SpawnPattern::Random:
		_createRandomPattern();
		break;
	case SpawnPattern::CircleCluster:
		_createCircleClusterPattern();
		break;
	case SpawnPattern::RectangleCluster:
		_createRectangleClusterPattern();
		break;
	}

	// for (int y = _screenHeight * 0.2f; y < _screenHeight * 0.7f; y += 14.0f) {
	// 	for (int x = _screenWidth * 0.2f; x < _screenWidth * 0.7f; x += 14.0f) {
	// 		const float radius = static_cast<float>(rand() % radiusCount + _minimumBallRadius);
	// 		_addBall(x, y, radius);
	// 	}
	// }

	//_addLine(30.0f, 30.0f, 30.0f, 920.0f, 4.0f);
	//_addLine(1000.0f, 30.0f, 1000.0f, 920.0f, 4.0f);
	//_addLine(30.0f, 920.0f, 1000.0f, 920.0f, 4.0f);
	_collisionList.reserve(static_cast<std::size_t>(_ballNum) * 8);
}

void BallCollisionEngine::_createRandomPattern() {
	const int radiusCount = _maximumBallRadius - _minimumBallRadius + 1;
	for (int i = 0; i < _ballNum; ++i) {
		const float radius = static_cast<float>(
			rand() % radiusCount + _minimumBallRadius
		);
		_addBall(
			static_cast<float>(rand() % _screenWidth),
			static_cast<float>(rand() % _screenHeight),
			radius
		);
	}
}

void BallCollisionEngine::_createCircleClusterPattern() {
	if (_ballNum <= 0)
		return;

	const float radius = static_cast<float>(_minimumBallRadius);
	const float diameter = radius * 2.0f;
	const float rowSpacing = std::sqrt(3.0f) * radius;
	const int extent = static_cast<int>(
		std::ceil(std::sqrt(static_cast<float>(_ballNum)))
	) + 2;

	std::vector<ClusterPoint> candidates;
	const std::size_t side = static_cast<std::size_t>(extent * 2 + 1);
	candidates.reserve(side * side);

	for (int row = -extent; row <= extent; ++row) {
		const float rowOffset = std::abs(row) % 2 == 0 ? 0.0f : radius;
		for (int column = -extent; column <= extent; ++column) {
			const float x = column * diameter + rowOffset;
			const float y = row * rowSpacing;
			candidates.push_back({ x, y, x * x + y * y });
		}
	}

	std::sort(
		candidates.begin(),
		candidates.end(),
		[](const ClusterPoint& first, const ClusterPoint& second) {
			if (first.distanceSquared != second.distanceSquared)
				return first.distanceSquared < second.distanceSquared;
			if (first.y != second.y)
				return first.y < second.y;
			return first.x < second.x;
		}
	);

	const float availableRadius =
		std::min(_screenWidth, _screenHeight) * 0.5f - radius;
	const float requiredRadius = std::sqrt(
		candidates[static_cast<std::size_t>(_ballNum - 1)].distanceSquared
	);
	if (requiredRadius > availableRadius + 1.0e-4f) {
		throw std::runtime_error(
			"circle_cluster does not fit on screen; reduce num_balls or "
			"ball_radius_min."
		);
	}

	const float centerX = _screenWidth * 0.5f;
	const float centerY = _screenHeight * 0.5f;
	for (int index = 0; index < _ballNum; ++index) {
		const ClusterPoint& point =
			candidates[static_cast<std::size_t>(index)];
		_addBall(centerX + point.x, centerY + point.y, radius);
	}


	_addBall(_screenWidth / 16, centerY, radius);
}

void BallCollisionEngine::_createRectangleClusterPattern() {
	if (_ballNum <= 0)
		return;

	const float radius = static_cast<float>(_minimumBallRadius);
	const float diameter = radius * 2.0f;
	const int maximumColumns = static_cast<int>(
		std::floor(_screenWidth / diameter)
	);
	const int maximumRows = static_cast<int>(
		std::floor(_screenHeight / diameter)
	);
	if (maximumColumns <= 0
		|| maximumRows <= 0
		|| static_cast<std::size_t>(_ballNum)
			> static_cast<std::size_t>(maximumColumns) * maximumRows) {
		throw std::runtime_error(
			"rectangle_cluster does not fit on screen; reduce num_balls or "
			"ball_radius_min."
		);
	}

	const float aspectRatio =
		static_cast<float>(_screenWidth) / _screenHeight;
	const int minimumColumns =
		(_ballNum + maximumRows - 1) / maximumRows;
	const int targetColumns = static_cast<int>(
		std::round(std::sqrt(_ballNum * aspectRatio))
	);
	const int columns = std::clamp(
		targetColumns,
		minimumColumns,
		std::min(maximumColumns, _ballNum)
	);
	const int rows = (_ballNum + columns - 1) / columns;
	const float centerX = _screenWidth * 0.5f;
	const float centerY = _screenHeight * 0.5f;
	const float firstColumnX = centerX - (columns - 1) * radius;
	const float firstRowY = centerY - (rows - 1) * radius;

	int created = 0;
	for (int row = 0; row < rows; ++row) {
		const int rowBallCount = std::min(columns, _ballNum - created);
		const int firstUsedColumn = (columns - rowBallCount) / 2;
		for (int column = 0; column < rowBallCount; ++column) {
			_addBall(
				firstColumnX
					+ (firstUsedColumn + column) * diameter,
				firstRowY + row * diameter,
				radius
			);
			++created;
		}
	}
}

void BallCollisionEngine::Update(float& fElapsedTime) {
	_update(fElapsedTime, nullptr);
}

void BallCollisionEngine::Update(float& fElapsedTime, PhysicsMetrics& metrics) {
	_update(fElapsedTime, &metrics);
}

void BallCollisionEngine::CycleGravity() {
	switch (_gravity) {
	case Gravity::None:
		_gravity = Gravity::Normal;
		break;
	case Gravity::Normal:
		_gravity = Gravity::Center;
		break;
	case Gravity::Center:
		_gravity = Gravity::Bicenter;
		break;
	case Gravity::Bicenter:
		_gravity = Gravity::None;
		break;
	}
}

Gravity BallCollisionEngine::GetGravity() const {
	return _gravity;
}

void BallCollisionEngine::_update(float& fElapsedTime, PhysicsMetrics* metrics) {
	using Clock = std::chrono::steady_clock;

	// Break up the frame elapsed time into smaller deltas for each simulation update
	float simElapsedTime = fElapsedTime / _timeSplit;
	for (int i = 0; i < _timeSplit; i++)
	{
		Clock::time_point phaseStart;
		if (metrics)
			phaseStart = Clock::now();

		// Set all balls time to maximum for this epoch
		for (auto& ball : Balls)
			ball.remainingTime = simElapsedTime;

		_updatePositionByVelocity(fElapsedTime);

		if (metrics) {
			metrics->integrationSeconds += std::chrono::duration<double>(
				Clock::now() - phaseStart
			).count();
			metrics->integrationUpdates += Balls.size();
			phaseStart = Clock::now();
		}

		_staticCollision(metrics);

		if (metrics) {
			metrics->collisionPairs += _collisionList.size();
			phaseStart = Clock::now();
		}

		const std::uint64_t impulsesApplied = _dynamicCollision();
		_resolveBoundaryCollisions();

		if (metrics) {
			metrics->impulsesApplied += impulsesApplied;
			metrics->collisionResponseSeconds += std::chrono::duration<double>(
				Clock::now() - phaseStart
			).count();
		}
	}
}

void BallCollisionEngine::_updatePositionByVelocity(float& fElapsedTime) {
	const int ballCount = static_cast<int>(Balls.size());
#pragma omp parallel for if(ballCount >= 4096)
	for (int i = 0; i < ballCount; i++)
	{
		if (Balls[i].remainingTime > 0) {
			// start position
			Balls[i].sx = Balls[i].x;
			Balls[i].sy = Balls[i].y;

			float dx = 0.0f;
			float dy = 0.0f;

			switch (_gravity)
			{
			case Gravity::None:
				break;
			case Gravity::Normal:
				dx = 0.0f;
				dy = 300.0f;
				break;
			case Gravity::Center:
				dx = _screenWidth / 2 - Balls[i].x;
				dy = _screenHeight / 2 - Balls[i].y;
				break;
			case Gravity::Bicenter:
				if (Balls[i].x < _screenWidth / 2) {
					dx = _screenWidth * 0.25f - Balls[i].x;
					dy = _screenHeight * 0.5f - Balls[i].y;
				}
				else {
					dx = _screenWidth * 0.75f - Balls[i].x;
					dy = _screenHeight * 0.5f - Balls[i].y;
				}
				break;
			default:
				break;
			}

			// Add Drag to emulate rolling friction
			Balls[i].ax =
				-Balls[i].vx * _velocityDrag + _gravityStrength * dx;
			Balls[i].ay =
				-Balls[i].vy * _velocityDrag + _gravityStrength * dy;

			// Update ball physics
			Balls[i].vx += Balls[i].ax * Balls[i].remainingTime;
			Balls[i].vy += Balls[i].ay * Balls[i].remainingTime;
			Balls[i].x += Balls[i].vx * Balls[i].remainingTime;
			Balls[i].y += Balls[i].vy * Balls[i].remainingTime;

			_resolveBoundaryCollision(Balls[i]);

			// Clamp velocity near zero
			if (std::fabs(Balls[i].vx * Balls[i].vx + Balls[i].vy * Balls[i].vy) < _minVelocity)
			{
				Balls[i].vx = 0;
				Balls[i].vy = 0;
			}
		}
	}
}

void BallCollisionEngine::_resolveBoundaryCollision(Ball& ball) const {
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

void BallCollisionEngine::_resolveBoundaryCollisions() {
	for (Ball& ball : Balls)
		_resolveBoundaryCollision(ball);
}

void BallCollisionEngine::_staticCollision(PhysicsMetrics* metrics) {
	using Clock = std::chrono::steady_clock;

	Clock::time_point phaseStart;
	if (metrics)
		phaseStart = Clock::now();

	_resolveLineCollisions();

	if (metrics) {
		metrics->collisionDetectionSeconds += std::chrono::duration<double>(
			Clock::now() - phaseStart
		).count();
		metrics->lineChecks += Balls.size() * Lines.size();
		phaseStart = Clock::now();
	}

	_buildSpatialGrid();

	if (metrics) {
		metrics->gridBuildSeconds += std::chrono::duration<double>(
			Clock::now() - phaseStart
		).count();
		phaseStart = Clock::now();
	}

	const std::uint64_t pairChecks = _resolveGridCollisions();
	_updateRemainingTimes();

	if (metrics) {
		const std::uint64_t ballCount = Balls.size();
		metrics->collisionDetectionSeconds += std::chrono::duration<double>(
			Clock::now() - phaseStart
		).count();
		metrics->ballPairChecks += pairChecks;
		metrics->possibleBallPairs += ballCount > 0
			? ballCount * (ballCount - 1) / 2
			: 0;
	}
}

void BallCollisionEngine::_resolveLineCollisions() {
	constexpr float DistanceEpsilon = 1.0e-6f;

	for (auto& ball : Balls) {
		for (auto& line : Lines) {
			float line_vector_x = line.ex - line.sx;
			float line_vector_y = line.ey - line.sy;
			float line_vector_sx = ball.x - line.sx;
			float line_vector_sy = ball.y - line.sy;
			float lengthSquared =
				line_vector_x * line_vector_x + line_vector_y * line_vector_y;
			if (lengthSquared <= DistanceEpsilon)
				continue;

			float t = std::clamp(
				(line_vector_x * line_vector_sx + line_vector_y * line_vector_sy)
					/ lengthSquared,
				0.0f,
				1.0f
			);
			float closest_x = line.sx + t * line_vector_x;
			float closest_y = line.sy + t * line_vector_y;
			float differenceX = ball.x - closest_x;
			float differenceY = ball.y - closest_y;
			float distanceSquared =
				differenceX * differenceX + differenceY * differenceY;
			float combinedRadius = ball.r + line.r;

			if (distanceSquared <= combinedRadius * combinedRadius)
			{
				float distance = std::sqrt(distanceSquared);
				float normalX;
				float normalY;
				if (distance > DistanceEpsilon) {
					normalX = differenceX / distance;
					normalY = differenceY / distance;
				}
				else {
					float lineLength = std::sqrt(lengthSquared);
					normalX = -line_vector_y / lineLength;
					normalY = line_vector_x / lineLength;
				}

				Ball fakeball{};
				fakeball.r = line.r;
				fakeball.mass = ball.mass * 0.8f;
				fakeball.x = closest_x;
				fakeball.y = closest_y;
				fakeball.vx = -ball.vx;
				fakeball.vy = -ball.vy;
				_fakeBalls.push_back(fakeball);

				_collisionList.push_back({ &ball, &_fakeBalls.back() });

				float penetration = combinedRadius - distance;
				ball.x += penetration * normalX;
				ball.y += penetration * normalY;
			}
		}
	}
}

void BallCollisionEngine::_buildSpatialGrid() {
	for (std::size_t cellIndex : _activeGridCells)
		_gridCellCounts[cellIndex] = 0;
	_activeGridCells.clear();

	if (Balls.empty()) {
		_gridBallCells.clear();
		_gridBallIndices.clear();
		return;
	}

	float maximumRadius = 0.5f;
	for (const Ball& ball : Balls)
		maximumRadius = std::max(maximumRadius, ball.r);

	_gridCellSize = std::max(1.0f, maximumRadius * 2.0f);
	const int columns = std::max(
		1,
		static_cast<int>(std::ceil(_screenWidth / _gridCellSize))
	);
	const int rows = std::max(
		1,
		static_cast<int>(std::ceil(_screenHeight / _gridCellSize))
	);

	if (columns != _gridColumns || rows != _gridRows) {
		_gridColumns = columns;
		_gridRows = rows;
		const std::size_t gridCellCount =
			static_cast<std::size_t>(_gridColumns) * _gridRows;
		_gridCellCounts.assign(gridCellCount, 0);
		_gridCellOffsets.resize(gridCellCount);
		_gridCellWriteOffsets.resize(gridCellCount);
		_activeGridCells.reserve(
			std::min(gridCellCount, Balls.size())
		);
	}

	_gridBallCells.resize(Balls.size());
	_gridBallIndices.resize(Balls.size());

	for (std::size_t index = 0; index < Balls.size(); ++index) {
		const Ball& ball = Balls[index];
		int column = std::clamp(
			static_cast<int>(ball.x / _gridCellSize),
			0,
			_gridColumns - 1
		);
		int row = std::clamp(
			static_cast<int>(ball.y / _gridCellSize),
			0,
			_gridRows - 1
		);

		const std::size_t cellIndex =
			static_cast<std::size_t>(row) * _gridColumns + column;
		_gridBallCells[index] = cellIndex;
		if (_gridCellCounts[cellIndex] == 0)
			_activeGridCells.push_back(cellIndex);
		++_gridCellCounts[cellIndex];
	}

	std::size_t packedOffset = 0;
	for (std::size_t cellIndex : _activeGridCells) {
		_gridCellOffsets[cellIndex] = packedOffset;
		_gridCellWriteOffsets[cellIndex] = packedOffset;
		packedOffset += _gridCellCounts[cellIndex];
	}

	for (std::size_t ballIndex = 0; ballIndex < Balls.size(); ++ballIndex) {
		const std::size_t cellIndex = _gridBallCells[ballIndex];
		_gridBallIndices[_gridCellWriteOffsets[cellIndex]++] = ballIndex;
	}
}

std::uint64_t BallCollisionEngine::_resolveGridCollisions() {
	static constexpr int NeighborOffsets[4][2] = {
		{1, 0},
		{-1, 1},
		{0, 1},
		{1, 1}
	};

	std::uint64_t pairChecks = 0;

	for (std::size_t cellIndex : _activeGridCells) {
		const int row = static_cast<int>(
			cellIndex / static_cast<std::size_t>(_gridColumns)
		);
		const int column = static_cast<int>(
			cellIndex % static_cast<std::size_t>(_gridColumns)
		);
		const std::size_t cellCount = _gridCellCounts[cellIndex];
		const std::size_t cellOffset = _gridCellOffsets[cellIndex];

		pairChecks += cellCount > 1
			? cellCount * (cellCount - 1) / 2
			: 0;
		for (std::size_t first = 0; first < cellCount; ++first) {
			for (
				std::size_t second = first + 1;
				second < cellCount;
				++second
			) {
				_resolveBallPair(
					_gridBallIndices[cellOffset + first],
					_gridBallIndices[cellOffset + second]
				);
			}
		}

		for (const auto& offset : NeighborOffsets) {
			const int neighborColumn = column + offset[0];
			const int neighborRow = row + offset[1];
			if (neighborColumn < 0
				|| neighborColumn >= _gridColumns
				|| neighborRow < 0
				|| neighborRow >= _gridRows) {
				continue;
			}

			const std::size_t neighborCellIndex =
				static_cast<std::size_t>(neighborRow) * _gridColumns
					+ neighborColumn;
			const std::size_t neighborCount =
				_gridCellCounts[neighborCellIndex];
			if (neighborCount == 0)
				continue;

			const std::size_t neighborOffset =
				_gridCellOffsets[neighborCellIndex];
			pairChecks += cellCount * neighborCount;

			for (std::size_t first = 0; first < cellCount; ++first) {
				for (
					std::size_t second = 0;
					second < neighborCount;
					++second
				) {
					_resolveBallPair(
						_gridBallIndices[cellOffset + first],
						_gridBallIndices[neighborOffset + second]
					);
				}
			}
		}
	}

	return pairChecks;
}

void BallCollisionEngine::_resolveBallPair(
	std::size_t firstIndex,
	std::size_t secondIndex
) {
	constexpr float DistanceEpsilon = 1.0e-6f;

	Ball& first = Balls[firstIndex];
	Ball& second = Balls[secondIndex];
	if (!_isCollision(
		first.x,
		first.y,
		first.r,
		second.x,
		second.y,
		second.r
	)) {
		return;
	}

	float differenceX = first.x - second.x;
	float differenceY = first.y - second.y;
	float distanceSquared =
		differenceX * differenceX + differenceY * differenceY;
	float distance;
	float normalX;
	float normalY;

	if (distanceSquared > DistanceEpsilon) {
		distance = std::sqrt(distanceSquared);
		normalX = differenceX / distance;
		normalY = differenceY / distance;
	}
	else {
		distance = 0.0f;
		normalX = firstIndex < secondIndex ? -1.0f : 1.0f;
		normalY = 0.0f;
	}

	const float penetration = first.r + second.r - distance;
	if (penetration > 0.0f) {
		const float correction = penetration * 0.5f;
		first.x += correction * normalX;
		first.y += correction * normalY;
		second.x -= correction * normalX;
		second.y -= correction * normalY;
	}

	_collisionList.push_back({ &first, &second });
}

void BallCollisionEngine::_updateRemainingTimes() {
	constexpr float SpeedEpsilon = 1.0e-8f;

	for (auto& ball : Balls) {
		float intendedSpeedSquared =
			ball.vx * ball.vx + ball.vy * ball.vy;
		if (intendedSpeedSquared <= SpeedEpsilon) {
			ball.remainingTime = 0.0f;
			continue;
		}

		float fIntendedSpeed = std::sqrt(intendedSpeedSquared);
		float fActualDistance = std::sqrt((ball.x - ball.sx) * (ball.x - ball.sx) + (ball.y - ball.sy) * (ball.y - ball.sy));
		float fActualTime = fActualDistance / fIntendedSpeed;
		ball.remainingTime -= fActualTime;
	}
}

std::uint64_t BallCollisionEngine::_dynamicCollision() {
	constexpr float DistanceEpsilon = 1.0e-6f;
	std::uint64_t impulsesApplied = 0;

	for (auto c : _collisionList)
	{
		Ball* b1 = c.first;
		Ball* b2 = c.second;

		// Distance between balls
		float diff_x = b1->x - b2->x;
		float diff_y = b1->y - b2->y;

		float distanceSquared = diff_x * diff_x + diff_y * diff_y;
		if (distanceSquared <= DistanceEpsilon)
			continue;

		float fDistance = std::sqrt(distanceSquared);

		// Normal
		float nx = diff_x / fDistance;
		float ny = diff_y / fDistance;

		float relativeNormalVelocity =
			nx * (b1->vx - b2->vx) + ny * (b1->vy - b2->vy);
		if (relativeNormalVelocity >= 0.0f)
			continue;

		float combinedMass = b1->mass + b2->mass;
		if (combinedMass <= 0.0f)
			continue;

		float p = 2.0f * relativeNormalVelocity / combinedMass;
		b1->vx = b1->vx - p * b2->mass * nx;
		b1->vy = b1->vy - p * b2->mass * ny;
		b2->vx = b2->vx + p * b1->mass * nx;
		b2->vy = b2->vy + p * b1->mass * ny;
		++impulsesApplied;
	}
	_collisionList.clear();
	_fakeBalls.clear();
	return impulsesApplied;
}
