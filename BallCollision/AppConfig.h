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
	SpawnPattern spawnPattern = SpawnPattern::Random;
};

AppConfig LoadAppConfig(const std::filesystem::path& path);
