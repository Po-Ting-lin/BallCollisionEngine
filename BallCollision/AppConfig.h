#pragma once

#include <filesystem>

#include "Gravity.h"
#include "SpawnPattern.h"

struct AppConfig {
	int numBalls = 3000;
	int minimumBallRadius = 2;
	int maximumBallRadius = 5;
	float rightDragLaunchSpeed = 60.0f;
	Gravity gravity = Gravity::None;
	float gravityStrength = 1.2f;
	float velocityDrag = 0.8f;
	float mouseForceStrength = 4000.0f;
	float mouseForceRadius = 200.0f;
	float wallRadius = 4.0f;
	SpawnPattern spawnPattern = SpawnPattern::Random;

	// Set from the command line, not from config.ini.
	bool frameBenchmark = false;
	std::filesystem::path frameBenchmarkOutput;
};

AppConfig LoadAppConfig(const std::filesystem::path& path);
