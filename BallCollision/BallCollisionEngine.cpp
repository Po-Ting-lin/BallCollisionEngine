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
	_mouseForceX = 0.0f;
	_mouseForceY = 0.0f;
	_mouseForceStrength = 0.0f;
	_mouseForceRadius = 0.0f;
	_substepContacts = 0;
	_substepImpulses = 0;
	_maximumBallRadiusThisSubstep = 0.5f;
	_spawnPattern = spawnPattern;
}

BallCollisionEngine::~BallCollisionEngine() {
	Balls.clear();
	Lines.clear();
	_cellBalls.clear();
	_gridCellCounts.clear();
	_gridCellOffsets.clear();
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

void BallCollisionEngine::Reset() {
	Balls.clear();
	Create();
}

void BallCollisionEngine::AddLine(
	float sx,
	float sy,
	float ex,
	float ey,
	float radius
) {
	_addLine(sx, sy, ex, ey, radius);
}

bool BallCollisionEngine::RemoveLineAt(float x, float y, float tolerance) {
	constexpr float DistanceEpsilon = 1.0e-6f;

	for (auto line = Lines.begin(); line != Lines.end(); ++line) {
		float line_vector_x = line->ex - line->sx;
		float line_vector_y = line->ey - line->sy;
		float lengthSquared =
			line_vector_x * line_vector_x + line_vector_y * line_vector_y;
		float t = lengthSquared > DistanceEpsilon
			? std::clamp(
				(line_vector_x * (x - line->sx)
					+ line_vector_y * (y - line->sy)) / lengthSquared,
				0.0f,
				1.0f
			)
			: 0.0f;
		float differenceX = x - (line->sx + t * line_vector_x);
		float differenceY = y - (line->sy + t * line_vector_y);
		float reach = line->r + tolerance;
		if (differenceX * differenceX + differenceY * differenceY
			<= reach * reach) {
			Lines.erase(line);
			return true;
		}
	}
	return false;
}

void BallCollisionEngine::ClearLines() {
	Lines.clear();
}

// Positive strength pulls balls toward (x, y), negative pushes them away.
// The force fades linearly to zero at the edge of the radius.
void BallCollisionEngine::SetMouseForce(
	float x,
	float y,
	float strength,
	float radius
) {
	_mouseForceX = x;
	_mouseForceY = y;
	_mouseForceStrength = strength;
	_mouseForceRadius = radius;
}

void BallCollisionEngine::ClearMouseForce() {
	_mouseForceStrength = 0.0f;
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

		_updatePositionByVelocity(simElapsedTime);

		if (metrics) {
			metrics->integrationSeconds += std::chrono::duration<double>(
				Clock::now() - phaseStart
			).count();
			metrics->integrationUpdates += Balls.size();
			phaseStart = Clock::now();
		}

		// Impulses are applied as contacts are found, inside _staticCollision.
		_substepContacts = 0;
		_substepImpulses = 0;
		_staticCollision(metrics);

		if (metrics) {
			metrics->collisionPairs += _substepContacts;
			metrics->impulsesApplied += _substepImpulses;
			phaseStart = Clock::now();
		}

		_resolveBoundaryCollisions();
		_scatterCellBalls();

		if (metrics) {
			metrics->collisionResponseSeconds += std::chrono::duration<double>(
				Clock::now() - phaseStart
			).count();
		}
	}
}

void BallCollisionEngine::_updatePositionByVelocity(float deltaTime) {
	// The grid needs the largest radius; finding it here saves a pass.
	float maximumRadius = 0.5f;
	const int ballCount = static_cast<int>(Balls.size());

	if (deltaTime <= 0.0f) {
		for (const Ball& ball : Balls)
			maximumRadius = std::max(maximumRadius, ball.r);
		_maximumBallRadiusThisSubstep = maximumRadius;
		return;
	}

#pragma omp parallel for if(ballCount >= 4096) reduction(max:maximumRadius)
	for (int i = 0; i < ballCount; i++)
	{
		maximumRadius = std::max(maximumRadius, Balls[i].r);
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

		if (_mouseForceStrength != 0.0f && _mouseForceRadius > 0.0f) {
			const float mouseDx = _mouseForceX - Balls[i].x;
			const float mouseDy = _mouseForceY - Balls[i].y;
			const float mouseDistanceSquared =
				mouseDx * mouseDx + mouseDy * mouseDy;
			if (mouseDistanceSquared < _mouseForceRadius * _mouseForceRadius
				&& mouseDistanceSquared > 1.0f) {
				const float mouseDistance = std::sqrt(mouseDistanceSquared);
				const float falloff =
					1.0f - mouseDistance / _mouseForceRadius;
				const float scale =
					_mouseForceStrength * falloff / mouseDistance;
				Balls[i].ax += mouseDx * scale;
				Balls[i].ay += mouseDy * scale;
			}
		}

		// Update ball physics
		Balls[i].vx += Balls[i].ax * deltaTime;
		Balls[i].vy += Balls[i].ay * deltaTime;
		Balls[i].x += Balls[i].vx * deltaTime;
		Balls[i].y += Balls[i].vy * deltaTime;

		_resolveBoundaryCollision(Balls[i]);

		// Clamp velocity near zero
		if (std::fabs(Balls[i].vx * Balls[i].vx + Balls[i].vy * Balls[i].vy) < _minVelocity)
		{
			Balls[i].vx = 0;
			Balls[i].vy = 0;
		}
	}
	_maximumBallRadiusThisSubstep = maximumRadius;
}

void BallCollisionEngine::_resolveBoundaryCollisions() {
	for (CellBall& ball : _cellBalls)
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
	_gatherCellBalls();

	if (metrics) {
		metrics->gridBuildSeconds += std::chrono::duration<double>(
			Clock::now() - phaseStart
		).count();
		phaseStart = Clock::now();
	}

	const std::uint64_t pairChecks = _resolveGridCollisions();

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

				float penetration = combinedRadius - distance;
				ball.x += penetration * normalX;
				ball.y += penetration * normalY;

				// Bounce as if hitting a stand-in ball at the closest point
				// with 0.8x this ball's mass and the opposite velocity.
				const float wallMass = ball.mass * 0.8f;
				const float relativeNormalVelocity =
					2.0f * (normalX * ball.vx + normalY * ball.vy);
				++_substepContacts;
				if (relativeNormalVelocity < 0.0f && ball.mass + wallMass > 0.0f) {
					const float p =
						2.0f * relativeNormalVelocity / (ball.mass + wallMass);
					ball.vx -= p * wallMass * normalX;
					ball.vy -= p * wallMass * normalY;
					++_substepImpulses;
				}
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

	_gridCellSize = std::max(1.0f, _maximumBallRadiusThisSubstep * 2.0f);
	const float inverseCellSize = 1.0f / _gridCellSize;
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
		_activeGridCells.reserve(
			std::min(gridCellCount, Balls.size())
		);
	}

	const std::uint32_t ballCount = static_cast<std::uint32_t>(Balls.size());
	const std::uint32_t gridColumns = static_cast<std::uint32_t>(_gridColumns);
	_gridBallCells.resize(ballCount);
	_gridBallRows.resize(ballCount);
	_gridBallIndices.resize(ballCount);
	_gridSortScratch.resize(ballCount);
	_gridColumnStarts.assign(static_cast<std::size_t>(_gridColumns) + 1, 0);
	_gridRowStarts.assign(static_cast<std::size_t>(_gridRows) + 1, 0);

	for (std::uint32_t index = 0; index < ballCount; ++index) {
		const Ball& ball = Balls[index];
		const std::uint32_t column = static_cast<std::uint32_t>(std::clamp(
			static_cast<int>(ball.x * inverseCellSize),
			0,
			_gridColumns - 1
		));
		const std::uint32_t row = static_cast<std::uint32_t>(std::clamp(
			static_cast<int>(ball.y * inverseCellSize),
			0,
			_gridRows - 1
		));

		_gridBallCells[index] = row * gridColumns + column;
		_gridBallRows[index] = row;
		++_gridColumnStarts[column + 1];
		++_gridRowStarts[row + 1];
	}
	for (int column = 0; column < _gridColumns; ++column)
		_gridColumnStarts[column + 1] += _gridColumnStarts[column];
	for (int row = 0; row < _gridRows; ++row)
		_gridRowStarts[row + 1] += _gridRowStarts[row];

	// Two stable counting-sort passes (column, then row) pack the balls in
	// row-major cell order, so neighbouring cells are close in memory. Balls
	// in one cell keep their Balls order.
	for (std::uint32_t index = 0; index < ballCount; ++index) {
		const std::uint32_t column =
			_gridBallCells[index] - _gridBallRows[index] * gridColumns;
		_gridSortScratch[_gridColumnStarts[column]++] = index;
	}
	for (std::uint32_t sorted = 0; sorted < ballCount; ++sorted) {
		const std::uint32_t index = _gridSortScratch[sorted];
		_gridBallIndices[_gridRowStarts[_gridBallRows[index]]++] = index;
	}

	for (std::uint32_t slot = 0; slot < ballCount; ++slot) {
		const std::uint32_t cellIndex = _gridBallCells[_gridBallIndices[slot]];
		if (_gridCellCounts[cellIndex] == 0) {
			_activeGridCells.push_back(cellIndex);
			_gridCellOffsets[cellIndex] = slot;
		}
		++_gridCellCounts[cellIndex];
	}
}

void BallCollisionEngine::_gatherCellBalls() {
	_cellBalls.resize(_gridBallIndices.size());
	for (std::size_t slot = 0; slot < _gridBallIndices.size(); ++slot) {
		const std::size_t ballIndex = _gridBallIndices[slot];
		const Ball& ball = Balls[ballIndex];
		CellBall& cellBall = _cellBalls[slot];
		cellBall.x = ball.x;
		cellBall.y = ball.y;
		cellBall.vx = ball.vx;
		cellBall.vy = ball.vy;
		cellBall.r = ball.r;
		cellBall.mass = ball.mass;
		cellBall.index = static_cast<std::uint32_t>(ballIndex);
	}
}

void BallCollisionEngine::_scatterCellBalls() {
	for (const CellBall& cellBall : _cellBalls) {
		Ball& ball = Balls[cellBall.index];
		ball.x = cellBall.x;
		ball.y = cellBall.y;
		ball.vx = cellBall.vx;
		ball.vy = cellBall.vy;
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

	for (std::uint32_t cellIndex : _activeGridCells) {
		const int row = static_cast<int>(cellIndex) / _gridColumns;
		const int column = static_cast<int>(cellIndex) - row * _gridColumns;
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
				_resolveBallPair(cellOffset + first, cellOffset + second);
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
					_resolveBallPair(cellOffset + first, neighborOffset + second);
				}
			}
		}
	}

	return pairChecks;
}

void BallCollisionEngine::_resolveBallPair(
	std::size_t firstSlot,
	std::size_t secondSlot
) {
	constexpr float DistanceEpsilon = 1.0e-6f;

	CellBall& first = _cellBalls[firstSlot];
	CellBall& second = _cellBalls[secondSlot];
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
		normalX = first.index < second.index ? -1.0f : 1.0f;
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

	// Elastic response along the contact normal, applied only to an
	// approaching pair.
	++_substepContacts;
	const float relativeNormalVelocity =
		normalX * (first.vx - second.vx) + normalY * (first.vy - second.vy);
	const float combinedMass = first.mass + second.mass;
	if (relativeNormalVelocity >= 0.0f || combinedMass <= 0.0f)
		return;

	const float p = 2.0f * relativeNormalVelocity / combinedMass;
	first.vx -= p * second.mass * normalX;
	first.vy -= p * second.mass * normalY;
	second.vx += p * first.mass * normalX;
	second.vy += p * first.mass * normalY;
	++_substepImpulses;
}
