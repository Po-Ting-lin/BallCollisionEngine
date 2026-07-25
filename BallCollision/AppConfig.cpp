#include "AppConfig.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
	std::string trim(const std::string& value) {
		std::size_t first = 0;
		while (first < value.size()
			&& std::isspace(static_cast<unsigned char>(value[first]))) {
			++first;
		}

		std::size_t last = value.size();
		while (last > first
			&& std::isspace(static_cast<unsigned char>(value[last - 1]))) {
			--last;
		}

		return value.substr(first, last - first);
	}

	std::string lowercase(std::string value) {
		for (char& character : value) {
			character = static_cast<char>(
				std::tolower(static_cast<unsigned char>(character))
			);
		}
		return value;
	}

	int parseInteger(
		const std::string& value,
		const std::string& key,
		std::size_t lineNumber
	) {
		std::size_t parsedCharacters = 0;
		int result;
		try {
			result = std::stoi(value, &parsedCharacters);
		}
		catch (const std::exception&) {
			throw std::runtime_error(
				"Invalid integer for '" + key + "' on config line "
					+ std::to_string(lineNumber) + "."
			);
		}

		if (parsedCharacters != value.size()) {
			throw std::runtime_error(
				"Invalid integer for '" + key + "' on config line "
					+ std::to_string(lineNumber) + "."
			);
		}
		return result;
	}

	float parseFloat(
		const std::string& value,
		const std::string& key,
		std::size_t lineNumber
	) {
		std::size_t parsedCharacters = 0;
		float result;
		try {
			result = std::stof(value, &parsedCharacters);
		}
		catch (const std::exception&) {
			throw std::runtime_error(
				"Invalid number for '" + key + "' on config line "
					+ std::to_string(lineNumber) + "."
			);
		}

		if (parsedCharacters != value.size()) {
			throw std::runtime_error(
				"Invalid number for '" + key + "' on config line "
					+ std::to_string(lineNumber) + "."
			);
		}
		return result;
	}

	Gravity parseGravity(
		const std::string& value,
		std::size_t lineNumber
	) {
		const std::string normalized = lowercase(value);
		if (normalized == "none")
			return Gravity::None;
		if (normalized == "normal")
			return Gravity::Normal;
		if (normalized == "center")
			return Gravity::Center;
		if (normalized == "bicenter")
			return Gravity::Bicenter;

		throw std::runtime_error(
			"Invalid gravity value on config line "
				+ std::to_string(lineNumber)
				+ ". Expected none, normal, center, or bicenter."
		);
	}

	SpawnPattern parseSpawnPattern(
		const std::string& value,
		std::size_t lineNumber
	) {
		const std::string normalized = lowercase(value);
		if (normalized == "random")
			return SpawnPattern::Random;
		if (normalized == "circle_cluster"
			|| normalized == "ball_cluster") {
			return SpawnPattern::CircleCluster;
		}
		if (normalized == "rectangle_cluster"
			|| normalized == "rect_cluster") {
			return SpawnPattern::RectangleCluster;
		}

		throw std::runtime_error(
			"Invalid pattern_mode value on config line "
				+ std::to_string(lineNumber)
				+ ". Expected random, circle_cluster, or rectangle_cluster."
		);
	}
}

AppConfig LoadAppConfig(const std::filesystem::path& path) {
	std::ifstream input(path);
	if (!input) {
		throw std::runtime_error(
			"Unable to open config file: " + path.string()
		);
	}

	AppConfig config;
	std::string line;
	std::size_t lineNumber = 0;

	while (std::getline(input, line)) {
		++lineNumber;
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;

		const std::size_t separator = line.find('=');
		if (separator == std::string::npos) {
			throw std::runtime_error(
				"Expected key=value on config line "
					+ std::to_string(lineNumber) + "."
			);
		}

		const std::string key = trim(line.substr(0, separator));
		const std::string value = trim(line.substr(separator + 1));
		if (key.empty() || value.empty()) {
			throw std::runtime_error(
				"Expected a non-empty key and value on config line "
					+ std::to_string(lineNumber) + "."
			);
		}

		if (key == "num_balls") {
			config.numBalls = parseInteger(value, key, lineNumber);
		}
		else if (key == "ball_radius_min") {
			config.minimumBallRadius =
				parseInteger(value, key, lineNumber);
		}
		else if (key == "ball_radius_max") {
			config.maximumBallRadius =
				parseInteger(value, key, lineNumber);
		}
		else if (key == "right_drag_launch_speed") {
			config.rightDragLaunchSpeed =
				parseFloat(value, key, lineNumber);
		}
		else if (key == "gravity") {
			config.gravity = parseGravity(value, lineNumber);
		}
		else if (key == "gravity_strength") {
			config.gravityStrength = parseFloat(value, key, lineNumber);
		}
		else if (key == "velocity_drag") {
			config.velocityDrag = parseFloat(value, key, lineNumber);
		}
		else if (key == "pattern_mode") {
			config.spawnPattern = parseSpawnPattern(value, lineNumber);
		}
		else {
			throw std::runtime_error(
				"Unknown config key '" + key + "' on line "
					+ std::to_string(lineNumber) + "."
			);
		}
	}

	if (config.numBalls < 0)
		throw std::runtime_error("num_balls must be zero or greater.");
	if (config.minimumBallRadius <= 0)
		throw std::runtime_error("ball_radius_min must be greater than zero.");
	if (config.maximumBallRadius < config.minimumBallRadius) {
		throw std::runtime_error(
			"ball_radius_max must be greater than or equal to ball_radius_min."
		);
	}
	if (!std::isfinite(config.rightDragLaunchSpeed)
		|| config.rightDragLaunchSpeed < 0.0f) {
		throw std::runtime_error(
			"right_drag_launch_speed must be zero or greater."
		);
	}
	if (!std::isfinite(config.gravityStrength)
		|| config.gravityStrength < 0.0f) {
		throw std::runtime_error(
			"gravity_strength must be zero or greater."
		);
	}
	if (!std::isfinite(config.velocityDrag)
		|| config.velocityDrag < 0.0f) {
		throw std::runtime_error(
			"velocity_drag must be zero or greater."
		);
	}

	return config;
}
