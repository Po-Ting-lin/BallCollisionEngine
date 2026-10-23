#include "BallCollisionEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
	Ball makeBall(
		int id,
		float x,
		float y,
		float radius,
		float velocityX = 0.0f,
		float velocityY = 0.0f
	) {
		Ball ball{};
		ball.id = id;
		ball.x = x;
		ball.y = y;
		ball.vx = velocityX;
		ball.vy = velocityY;
		ball.r = radius;
		ball.mass = 10.0f * radius;
		return ball;
	}

	bool require(bool condition, const std::string& message) {
		if (!condition)
			std::cerr << "FAILED: " << message << '\n';
		return condition;
	}

	bool isTightlyPackedAtRadius(
		const std::vector<Ball>& balls,
		float expectedRadius,
		std::size_t packedBallCount
	) {
		if (packedBallCount < 2)
			return true;
		if (balls.size() < packedBallCount)
			return false;

		const float diameter = expectedRadius * 2.0f;
		constexpr float Tolerance = 1.0e-3f;
		for (std::size_t first = 0; first < packedBallCount; ++first) {
			if (balls[first].r != expectedRadius)
				return false;

			float nearestDistance = std::numeric_limits<float>::max();
			for (std::size_t second = 0; second < packedBallCount; ++second) {
				if (first == second)
					continue;

				const float distance = std::hypot(
					balls[first].x - balls[second].x,
					balls[first].y - balls[second].y
				);
				if (distance < diameter - Tolerance)
					return false;
				nearestDistance = std::min(nearestDistance, distance);
			}

			if (std::fabs(nearestDistance - diameter) > Tolerance)
				return false;
		}
		return true;
	}

	bool testSparseGridCullsDistantPair() {
		BallCollisionEngine engine(100, 100, 0);
		engine.Balls.push_back(makeBall(0, 10.0f, 10.0f, 2.0f, 1.0f));
		engine.Balls.push_back(makeBall(1, 90.0f, 90.0f, 2.0f, -1.0f));

		float elapsedTime = 0.0f;
		PhysicsMetrics metrics;
		engine.Update(elapsedTime, metrics);

		return require(
			metrics.possibleBallPairs == 4,
			"two balls should have one possible pair in each of four substeps"
		) && require(
			metrics.ballPairChecks == 0,
			"distant non-neighboring cells should produce no candidate checks"
		);
	}

	bool testCollisionAcrossCellBoundary() {
		BallCollisionEngine engine(100, 100, 0);
		engine.Balls.push_back(makeBall(0, 3.5f, 10.0f, 1.0f, 1.0f));
		engine.Balls.push_back(makeBall(1, 4.5f, 10.0f, 1.0f, -1.0f));

		float elapsedTime = 0.0f;
		PhysicsMetrics metrics;
		engine.Update(elapsedTime, metrics);

		const Ball& first = engine.Balls[0];
		const Ball& second = engine.Balls[1];
		const float separation = std::fabs(first.x - second.x);

		return require(
			separation + 1.0e-4f >= first.r + second.r,
			"overlap across adjacent cells should be corrected"
		) && require(
			first.vx < 0.0f && second.vx > 0.0f,
			"approaching balls should receive one elastic response"
		) && require(
			metrics.ballPairChecks == metrics.possibleBallPairs,
			"the adjacent isolated pair should be checked once per substep"
		);
	}

	bool testCoincidentCentersRemainFinite() {
		BallCollisionEngine engine(100, 100, 0);
		engine.Balls.push_back(makeBall(0, 50.0f, 50.0f, 2.0f));
		engine.Balls.push_back(makeBall(1, 50.0f, 50.0f, 2.0f));

		float elapsedTime = 0.0f;
		engine.Update(elapsedTime);

		const Ball& first = engine.Balls[0];
		const Ball& second = engine.Balls[1];
		const float separation = std::hypot(
			first.x - second.x,
			first.y - second.y
		);

		return require(
			std::isfinite(first.x)
				&& std::isfinite(first.y)
				&& std::isfinite(second.x)
				&& std::isfinite(second.y),
			"coincident centers must not create NaN positions"
		) && require(
			separation + 1.0e-4f >= first.r + second.r,
			"coincident balls should be separated deterministically"
		);
	}

	bool testDiagonalNeighborCollision() {
		BallCollisionEngine engine(100, 100, 0);
		engine.Balls.push_back(
			makeBall(0, 3.5f, 3.5f, 1.0f, 1.0f, 1.0f)
		);
		engine.Balls.push_back(
			makeBall(1, 4.5f, 4.5f, 1.0f, -1.0f, -1.0f)
		);

		float elapsedTime = 0.0f;
		engine.Update(elapsedTime);

		const Ball& first = engine.Balls[0];
		const Ball& second = engine.Balls[1];
		const float separation = std::hypot(
			first.x - second.x,
			first.y - second.y
		);

		return require(
			separation + 1.0e-4f >= first.r + second.r,
			"overlap across diagonal grid neighbors should be corrected"
		);
	}

	bool testCandidatesAreUnique() {
		BallCollisionEngine engine(100, 100, 0);
		engine.Balls.push_back(makeBall(0, 10.0f, 10.0f, 8.0f, 1.0f));
		engine.Balls.push_back(makeBall(1, 20.0f, 10.0f, 8.0f, -1.0f));
		engine.Balls.push_back(makeBall(2, 35.0f, 10.0f, 8.0f, -1.0f));

		float elapsedTime = 0.0f;
		PhysicsMetrics metrics;
		engine.Update(elapsedTime, metrics);

		return require(
			metrics.ballPairChecks <= metrics.possibleBallPairs,
			"grid traversal must never emit duplicate candidate pairs"
		);
	}

	bool testVacatedActiveCellIsCleared() {
		BallCollisionEngine engine(100, 100, 0);
		engine.Balls.push_back(makeBall(0, 1.5f, 10.0f, 1.0f));
		engine.Balls.push_back(makeBall(1, 2.5f, 10.0f, 1.0f));

		float elapsedTime = 0.0f;
		PhysicsMetrics initialMetrics;
		engine.Update(elapsedTime, initialMetrics);

		engine.Balls[1].x = 90.0f;
		engine.Balls[1].y = 90.0f;
		PhysicsMetrics movedMetrics;
		engine.Update(elapsedTime, movedMetrics);

		return require(
			initialMetrics.ballPairChecks > 0,
			"adjacent occupied cells should initially produce candidates"
		) && require(
			movedMetrics.ballPairChecks == 0,
			"vacated active cells must not retain stale ball links"
		);
	}

	bool testEmptyGridCanBePopulatedLater() {
		BallCollisionEngine engine(100, 100, 0);
		float elapsedTime = 0.0f;
		engine.Update(elapsedTime);

		engine.Balls.push_back(makeBall(0, 3.5f, 10.0f, 1.0f));
		engine.Balls.push_back(makeBall(1, 4.5f, 10.0f, 1.0f));
		engine.Update(elapsedTime);

		const Ball& first = engine.Balls[0];
		const Ball& second = engine.Balls[1];
		return require(
			std::fabs(first.x - second.x) + 1.0e-4f
				>= first.r + second.r,
			"an initially empty active grid should accept later ball insertion"
		);
	}

	bool testConfiguredBallCountAndRadius() {
		constexpr int ExpectedBallCount = 17;
		constexpr float ExpectedRadius = 7.0f;
		BallCollisionEngine engine(
			100,
			100,
			ExpectedBallCount,
			static_cast<int>(ExpectedRadius),
			static_cast<int>(ExpectedRadius)
		);
		engine.Create();

		bool radiiMatch = true;
		for (const Ball& ball : engine.Balls)
			radiiMatch = radiiMatch && ball.r == ExpectedRadius;

		return require(
			engine.Balls.size() == static_cast<std::size_t>(ExpectedBallCount),
			"configured ball count should control scene creation"
		) && require(
			radiiMatch,
			"equal configured radius bounds should create fixed-size balls"
		);
	}

	bool testCircleClusterUsesTightMinimumRadiusPacking() {
		constexpr float ExpectedRadius = 2.0f;
		constexpr std::size_t ExpectedClusterSize = 61;
		BallCollisionEngine engine(
			200,
			100,
			static_cast<int>(ExpectedClusterSize),
			static_cast<int>(ExpectedRadius),
			7,
			Gravity::None,
			1.2f,
			0.0f,
			SpawnPattern::CircleCluster
		);
		engine.Create();

		bool insideScreen = true;
		for (const Ball& ball : engine.Balls) {
			insideScreen = insideScreen
				&& ball.x - ball.r >= 0.0f
				&& ball.x + ball.r <= 200.0f
				&& ball.y - ball.r >= 0.0f
				&& ball.y + ball.r <= 100.0f;
		}

		return require(
			isTightlyPackedAtRadius(
				engine.Balls,
				ExpectedRadius,
				ExpectedClusterSize
			),
			"circle cluster should be hex-packed using ball_radius_min"
		) && require(
			insideScreen,
			"packed circle cluster should stay inside the screen"
		);
	}

	bool testRectangleClusterUsesTightMinimumRadiusPacking() {
		constexpr float ExpectedRadius = 2.0f;
		constexpr std::size_t ExpectedClusterSize = 60;
		BallCollisionEngine engine(
			200,
			100,
			static_cast<int>(ExpectedClusterSize),
			static_cast<int>(ExpectedRadius),
			7,
			Gravity::None,
			1.2f,
			0.0f,
			SpawnPattern::RectangleCluster
		);
		engine.Create();

		bool insideScreen = true;
		for (const Ball& ball : engine.Balls) {
			insideScreen = insideScreen
				&& ball.x - ball.r >= 0.0f
				&& ball.x + ball.r <= 200.0f
				&& ball.y - ball.r >= 0.0f
				&& ball.y + ball.r <= 100.0f;
		}

		return require(
			isTightlyPackedAtRadius(
				engine.Balls,
				ExpectedRadius,
				ExpectedClusterSize
			),
			"rectangle cluster should be grid-packed using ball_radius_min"
		) && require(
			insideScreen,
			"packed rectangle cluster should stay inside the screen"
		);
	}

	bool testZeroDragPreservesVelocity() {
		BallCollisionEngine engine(
			200,
			200,
			0,
			2,
			5,
			Gravity::None,
			1.2f,
			0.0f
		);
		engine.Balls.push_back(
			makeBall(0, 100.0f, 100.0f, 2.0f, 24.0f, -12.0f)
		);

		float elapsedTime = 1.0f / 60.0f;
		engine.Update(elapsedTime);

		const Ball& ball = engine.Balls.front();
		return require(
			std::fabs(ball.vx - 24.0f) < 1.0e-4f
				&& std::fabs(ball.vy + 12.0f) < 1.0e-4f,
			"zero drag should preserve velocity between collisions"
		) && require(
			std::fabs(ball.ax) < 1.0e-4f
				&& std::fabs(ball.ay) < 1.0e-4f,
			"none gravity with zero drag should apply no acceleration"
		);
	}

	bool testVelocityDragSlowsBall() {
		BallCollisionEngine engine(
			200,
			200,
			0,
			2,
			5,
			Gravity::None,
			1.2f,
			2.0f
		);
		engine.Balls.push_back(
			makeBall(0, 100.0f, 100.0f, 2.0f, 24.0f, 0.0f)
		);

		float elapsedTime = 1.0f / 60.0f;
		engine.Update(elapsedTime);

		const Ball& ball = engine.Balls.front();
		return require(
			ball.vx > 0.0f && ball.vx < 24.0f,
			"positive velocity drag should slow a moving ball"
		);
	}

	bool testBallsReflectInsideEveryScreenBoundary() {
		BallCollisionEngine engine(
			100,
			100,
			0,
			2,
			5,
			Gravity::None,
			0.0f,
			0.0f
		);
		engine.Balls.push_back(
			makeBall(0, 5.1f, 25.0f, 5.0f, -100.0f, 0.0f)
		);
		engine.Balls.push_back(
			makeBall(1, 94.9f, 45.0f, 5.0f, 100.0f, 0.0f)
		);
		engine.Balls.push_back(
			makeBall(2, 25.0f, 5.1f, 5.0f, 0.0f, -100.0f)
		);
		engine.Balls.push_back(
			makeBall(3, 65.0f, 94.9f, 5.0f, 0.0f, 100.0f)
		);

		float elapsedTime = 1.0f / 60.0f;
		engine.Update(elapsedTime);

		bool allInside = true;
		for (const Ball& ball : engine.Balls) {
			allInside = allInside
				&& ball.x >= ball.r
				&& ball.x <= 100.0f - ball.r
				&& ball.y >= ball.r
				&& ball.y <= 100.0f - ball.r;
		}

		return require(
			allInside,
			"boundary response should keep each complete ball on screen"
		) && require(
			engine.Balls[0].vx > 0.0f
				&& engine.Balls[1].vx < 0.0f
				&& engine.Balls[2].vy > 0.0f
				&& engine.Balls[3].vy < 0.0f,
			"outward velocity should reflect at every screen boundary"
		);
	}

	bool testBoundaryDoesNotReflectInwardVelocity() {
		BallCollisionEngine engine(
			100,
			100,
			0,
			2,
			5,
			Gravity::None,
			0.0f,
			0.0f
		);
		engine.Balls.push_back(
			makeBall(0, 4.0f, 50.0f, 5.0f, 10.0f, 0.0f)
		);

		float elapsedTime = 1.0f / 60.0f;
		engine.Update(elapsedTime);

		const Ball& ball = engine.Balls.front();
		return require(
			ball.x >= ball.r,
			"an outside ball should be clamped back inside"
		) && require(
			ball.vx > 0.0f,
			"an inward velocity must not be reflected back into the wall"
		);
	}

	bool testCollisionCorrectionCannotPushBallOutsideBoundary() {
		BallCollisionEngine engine(
			100,
			100,
			0,
			2,
			5,
			Gravity::None,
			0.0f,
			0.0f
		);
		engine.Balls.push_back(
			makeBall(0, 5.1f, 50.0f, 5.0f)
		);
		engine.Balls.push_back(
			makeBall(1, 7.0f, 50.0f, 5.0f)
		);

		float elapsedTime = 0.0f;
		engine.Update(elapsedTime);

		for (const Ball& ball : engine.Balls) {
			if (ball.x < ball.r || ball.x > 100.0f - ball.r) {
				return require(
					false,
					"collision correction must not leave a ball outside"
				);
			}
		}
		return true;
	}

	bool testGravityStrengthControlsAcceleration() {
		BallCollisionEngine zeroStrength(
			200,
			200,
			0,
			2,
			5,
			Gravity::Normal,
			0.0f
		);
		BallCollisionEngine positiveStrength(
			200,
			200,
			0,
			2,
			5,
			Gravity::Normal,
			2.0f
		);
		zeroStrength.Balls.push_back(
			makeBall(0, 100.0f, 100.0f, 2.0f)
		);
		positiveStrength.Balls.push_back(
			makeBall(0, 100.0f, 100.0f, 2.0f)
		);

		float elapsedTime = 1.0f / 60.0f;
		zeroStrength.Update(elapsedTime);
		positiveStrength.Update(elapsedTime);

		return require(
			std::fabs(zeroStrength.Balls.front().vy) < 1.0e-4f,
			"zero gravity strength should disable gravitational acceleration"
		) && require(
			positiveStrength.Balls.front().vy > 0.0f,
			"positive normal-gravity strength should accelerate balls downward"
		);
	}

	bool testGravityCyclesInConfiguredOrder() {
		BallCollisionEngine engine(
			100,
			100,
			0,
			2,
			5,
			Gravity::None
		);

		bool orderIsCorrect = engine.GetGravity() == Gravity::None;
		engine.CycleGravity();
		orderIsCorrect =
			orderIsCorrect && engine.GetGravity() == Gravity::Normal;
		engine.CycleGravity();
		orderIsCorrect =
			orderIsCorrect && engine.GetGravity() == Gravity::Center;
		engine.CycleGravity();
		orderIsCorrect =
			orderIsCorrect && engine.GetGravity() == Gravity::Bicenter;
		engine.CycleGravity();
		orderIsCorrect =
			orderIsCorrect && engine.GetGravity() == Gravity::None;

		return require(
			orderIsCorrect,
			"gravity should cycle none, normal, center, bicenter, none"
		);
	}

	bool testMouseForcePullsAndPushes() {
		BallCollisionEngine engine(
			400, 400, 0, 2, 2, Gravity::None, 0.0f, 0.0f
		);
		engine.Balls.push_back(makeBall(0, 150.0f, 200.0f, 2.0f));
		engine.Balls.push_back(makeBall(1, 300.0f, 200.0f, 2.0f));
		engine.SetMouseForce(200.0f, 200.0f, 1000.0f, 100.0f);

		float elapsedTime = 0.02f;
		engine.Update(elapsedTime);
		const bool pulled = engine.Balls[0].vx > 0.0f;
		const bool outsideUntouched = engine.Balls[1].vx == 0.0f;

		engine.Balls[0] = makeBall(0, 150.0f, 200.0f, 2.0f);
		engine.SetMouseForce(200.0f, 200.0f, -1000.0f, 100.0f);
		engine.Update(elapsedTime);
		const bool pushed = engine.Balls[0].vx < 0.0f;

		engine.Balls[0] = makeBall(0, 150.0f, 200.0f, 2.0f);
		engine.ClearMouseForce();
		engine.Update(elapsedTime);
		const bool cleared = engine.Balls[0].vx == 0.0f;

		return require(pulled, "positive mouse force should pull balls")
			&& require(pushed, "negative mouse force should push balls")
			&& require(
				outsideUntouched,
				"mouse force should not affect balls outside its radius"
			)
			&& require(cleared, "cleared mouse force should do nothing");
	}

	bool testLinesCanBeAddedAndRemoved() {
		BallCollisionEngine engine(400, 400, 0);
		engine.AddLine(100.0f, 100.0f, 300.0f, 100.0f, 4.0f);
		engine.AddLine(100.0f, 300.0f, 300.0f, 300.0f, 4.0f);

		const bool missIgnored = !engine.RemoveLineAt(200.0f, 200.0f, 4.0f)
			&& engine.Lines.size() == 2;
		const bool hitRemoved = engine.RemoveLineAt(200.0f, 305.0f, 4.0f)
			&& engine.Lines.size() == 1
			&& engine.Lines[0].sy == 100.0f;
		engine.ClearLines();

		return require(missIgnored, "removing away from a line should miss")
			&& require(hitRemoved, "removing near a line should erase it")
			&& require(engine.Lines.empty(), "ClearLines should remove all");
	}

	bool testBallsBounceOffWalls() {
		BallCollisionEngine engine(
			400, 400, 0, 2, 2, Gravity::None, 0.0f, 0.0f
		);
		engine.AddLine(200.0f, 100.0f, 200.0f, 300.0f, 4.0f);
		// several simultaneous contacts exercise the fake-ball storage
		for (int index = 0; index < 16; ++index) {
			engine.Balls.push_back(makeBall(
				index,
				194.5f,
				110.0f + index * 10.0f,
				2.0f,
				50.0f
			));
		}

		float elapsedTime = 1.0f / 60.0f;
		engine.Update(elapsedTime);

		bool allBounced = true;
		bool allClear = true;
		for (const Ball& ball : engine.Balls) {
			allBounced = allBounced && std::isfinite(ball.vx) && ball.vx < 0.0f;
			allClear = allClear && ball.x + ball.r <= 196.0f + 1.0e-3f;
		}

		return require(
			allBounced,
			"balls moving into a wall should be reflected away from it"
		) && require(
			allClear,
			"wall contact should push balls out of the wall"
		);
	}

	bool testResetRespawnsConfiguredBalls() {
		BallCollisionEngine engine(400, 400, 25);
		engine.Create();
		engine.Balls.pop_back();
		engine.AddLine(10.0f, 10.0f, 50.0f, 10.0f, 4.0f);
		engine.Reset();

		return require(
			engine.Balls.size() == 25,
			"Reset should respawn the configured ball count"
		) && require(engine.Lines.size() == 1, "Reset should keep walls");
	}
}

int main() {
	bool passed = true;
	passed = testSparseGridCullsDistantPair() && passed;
	passed = testCollisionAcrossCellBoundary() && passed;
	passed = testCoincidentCentersRemainFinite() && passed;
	passed = testDiagonalNeighborCollision() && passed;
	passed = testCandidatesAreUnique() && passed;
	passed = testVacatedActiveCellIsCleared() && passed;
	passed = testEmptyGridCanBePopulatedLater() && passed;
	passed = testConfiguredBallCountAndRadius() && passed;
	passed = testCircleClusterUsesTightMinimumRadiusPacking() && passed;
	passed = testRectangleClusterUsesTightMinimumRadiusPacking() && passed;
	passed = testZeroDragPreservesVelocity() && passed;
	passed = testVelocityDragSlowsBall() && passed;
	passed = testBallsReflectInsideEveryScreenBoundary() && passed;
	passed = testBoundaryDoesNotReflectInwardVelocity() && passed;
	passed = testCollisionCorrectionCannotPushBallOutsideBoundary() && passed;
	passed = testGravityStrengthControlsAcceleration() && passed;
	passed = testGravityCyclesInConfiguredOrder() && passed;
	passed = testMouseForcePullsAndPushes() && passed;
	passed = testLinesCanBeAddedAndRemoved() && passed;
	passed = testBallsBounceOffWalls() && passed;
	passed = testResetRespawnsConfiguredBalls() && passed;

	if (!passed)
		return EXIT_FAILURE;

	std::cout << "All BallCollision grid tests passed.\n";
	return EXIT_SUCCESS;
}
