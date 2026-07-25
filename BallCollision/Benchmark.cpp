#include "BallCollisionEngine.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <omp.h>

namespace {
	constexpr float FrameTime = 1.0f / 60.0f;

	struct BenchmarkCase {
		const char* scenario;
		int ballCount;
		int screenWidth;
		int screenHeight;
		int minimumRadius;
		int maximumRadius;
		int warmupFrames;
		int measuredFramesPerBatch;
		int batches;
	};

	struct BenchmarkResult {
		std::string scenario;
		int ballCount = 0;
		std::uint64_t measuredFrames = 0;
		double wallSeconds = 0.0;
		PhysicsMetrics metrics;
		std::uint64_t invalidBallStates = 0;
	};

	void initializeDeterministicScene(
		BallCollisionEngine& engine,
		const BenchmarkCase& benchmarkCase,
		std::uint32_t seed
	) {
		std::srand(seed);
		engine.Create();

		std::mt19937 generator(seed);
		std::uniform_real_distribution<float> xDistribution(
			0.0f,
			static_cast<float>(benchmarkCase.screenWidth)
		);
		std::uniform_real_distribution<float> yDistribution(
			0.0f,
			static_cast<float>(benchmarkCase.screenHeight)
		);
		std::uniform_int_distribution<int> radiusDistribution(
			benchmarkCase.minimumRadius,
			benchmarkCase.maximumRadius
		);

		for (std::size_t index = 0; index < engine.Balls.size(); ++index) {
			Ball& ball = engine.Balls[index];
			const float radius = static_cast<float>(radiusDistribution(generator));

			ball.id = static_cast<int>(index);
			ball.x = xDistribution(generator);
			ball.y = yDistribution(generator);
			ball.sx = ball.x;
			ball.sy = ball.y;
			ball.vx = 0.0f;
			ball.vy = 0.0f;
			ball.ax = 0.0f;
			ball.ay = 0.0f;
			ball.r = radius;
			ball.mass = 10.0f * radius;
			ball.remainingTime = 0.0f;
			ball.color = 0.0f;
		}
	}

	std::uint64_t countInvalidBallStates(const BallCollisionEngine& engine) {
		std::uint64_t invalidStates = 0;
		for (const Ball& ball : engine.Balls) {
			if (!std::isfinite(ball.x)
				|| !std::isfinite(ball.y)
				|| !std::isfinite(ball.vx)
				|| !std::isfinite(ball.vy)) {
				++invalidStates;
			}
		}
		return invalidStates;
	}

	BenchmarkResult runCase(const BenchmarkCase& benchmarkCase) {
		using Clock = std::chrono::steady_clock;

		BenchmarkResult result;
		result.scenario = benchmarkCase.scenario;
		result.ballCount = benchmarkCase.ballCount;

		for (int batch = 0; batch < benchmarkCase.batches; ++batch) {
			BallCollisionEngine engine(
				benchmarkCase.screenWidth,
				benchmarkCase.screenHeight,
				benchmarkCase.ballCount
			);
			initializeDeterministicScene(
				engine,
				benchmarkCase,
				0xC0FFEEu + static_cast<std::uint32_t>(batch)
			);

			float elapsedTime = FrameTime;
			for (int frame = 0; frame < benchmarkCase.warmupFrames; ++frame) {
				engine.Update(elapsedTime);
			}

			const auto start = Clock::now();
			for (
				int frame = 0;
				frame < benchmarkCase.measuredFramesPerBatch;
				++frame
			) {
				engine.Update(elapsedTime, result.metrics);
			}
			result.wallSeconds += std::chrono::duration<double>(
				Clock::now() - start
			).count();

			result.measuredFrames += benchmarkCase.measuredFramesPerBatch;
			result.invalidBallStates += countInvalidBallStates(engine);
		}

		return result;
	}

	double millionsPerSecond(std::uint64_t operations, double seconds) {
		if (seconds <= 0.0)
			return 0.0;
		return static_cast<double>(operations) / seconds / 1'000'000.0;
	}

	void printResult(const BenchmarkResult& result) {
		const double frames = static_cast<double>(result.measuredFrames);
		const double stageSeconds =
			result.metrics.integrationSeconds
			+ result.metrics.gridBuildSeconds
			+ result.metrics.collisionDetectionSeconds
			+ result.metrics.collisionResponseSeconds;
		const double collisionShare = stageSeconds > 0.0
			? 100.0 * (
				result.metrics.gridBuildSeconds
				+ result.metrics.collisionDetectionSeconds
			) / stageSeconds
			: 0.0;
		const double culledShare = result.metrics.possibleBallPairs > 0
			? 100.0 * (
				1.0 - static_cast<double>(result.metrics.ballPairChecks)
					/ result.metrics.possibleBallPairs
			)
			: 0.0;

		std::cout
			<< "| " << result.scenario
			<< " | " << result.ballCount
			<< " | " << result.measuredFrames
			<< " | " << 1'000.0 * result.wallSeconds / frames
			<< " | " << frames / result.wallSeconds
			<< " | " << 1'000.0 * result.metrics.integrationSeconds / frames
			<< " | " << 1'000.0 * result.metrics.gridBuildSeconds / frames
			<< " | " << 1'000.0 * result.metrics.collisionDetectionSeconds / frames
			<< " | " << 1'000'000.0 * result.metrics.collisionResponseSeconds / frames
			<< " | " << millionsPerSecond(
				result.metrics.integrationUpdates,
				result.metrics.integrationSeconds
			)
			<< " | " << millionsPerSecond(
				result.metrics.ballPairChecks,
				result.metrics.collisionDetectionSeconds
			)
			<< " | " << millionsPerSecond(
				result.metrics.collisionPairs,
				result.metrics.collisionResponseSeconds
			)
			<< " | " << static_cast<double>(
				result.metrics.ballPairChecks
			) / frames
			<< " | " << culledShare
			<< " | " << static_cast<double>(
				result.metrics.collisionPairs
			) / frames
			<< " | " << static_cast<double>(
				result.metrics.impulsesApplied
			) / frames
			<< " | " << collisionShare
			<< " | " << result.invalidBallStates
			<< " |\n";
	}
}

int main() {
	omp_set_dynamic(0);

	const std::vector<BenchmarkCase> cases = {
		{"baseline", 100, 2'048, 1'200, 5, 16, 10, 120, 20},
		{"baseline", 300, 2'048, 1'200, 5, 16, 10, 120, 5},
		{"baseline", 1'000, 2'048, 1'200, 5, 16, 10, 120, 1},
		{"baseline", 2'000, 2'048, 1'200, 5, 16, 10, 30, 1},
		{"grid scale", 5'000, 2'048, 1'200, 5, 16, 10, 30, 1},
		{"grid scale", 10'000, 2'048, 1'200, 5, 16, 10, 15, 1},
		{"small-radius grid", 5'000, 1'920, 1'080, 1, 3, 10, 60, 1},
		{"current app", 1'000, 1'920, 1'080, 1, 12, 10, 120, 1}
	};

	std::cout << "BallCollision physics throughput benchmark\n";
	std::cout << "Compiler: " << __VERSION__ << "\n";
#ifdef NDEBUG
	std::cout << "Build: Release\n";
#else
	std::cout << "Build: Debug\n";
#endif
	std::cout << "OpenMP max threads: " << omp_get_max_threads() << "\n";
	std::cout << "OpenMP processors: " << omp_get_num_procs() << "\n";
	std::cout << "Frame time: " << FrameTime << " seconds\n";
	std::cout << "Scene: deterministic continuous positions\n\n";

	std::cout << std::fixed << std::setprecision(3);
	std::cout
		<< "| Scenario | Balls | Frames | Total ms/frame | Physics FPS"
		<< " | Integrate ms/frame | Grid ms/frame | Candidate ms/frame"
		<< " | Respond us/frame | Integrate M updates/s"
		<< " | Candidate M checks/s | Respond M contacts/s"
		<< " | Candidates/frame | Culled % | Contacts/frame"
		<< " | Applied impulses/frame | Collision share % | Invalid states |\n";
	std::cout
		<< "|:---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";

	for (const BenchmarkCase& benchmarkCase : cases) {
		printResult(runCase(benchmarkCase));
	}

	return 0;
}
