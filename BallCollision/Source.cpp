#include "UI_mainConsole.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[])
{
	try {
		std::filesystem::path configPath = "config.ini";
		if (!std::filesystem::exists(configPath) && argc > 0) {
			configPath = std::filesystem::absolute(argv[0])
				.parent_path()
				/ "config.ini";
		}

		AppConfig config = LoadAppConfig(configPath);
		for (int index = 1; index < argc; ++index) {
			const std::string argument = argv[index];
			if (argument == "--frame-benchmark") {
				config.frameBenchmark = true;
				if (index + 1 < argc
					&& std::string(argv[index + 1]).rfind("--", 0) != 0) {
					config.frameBenchmarkOutput = argv[++index];
				}
			}
			else {
				throw std::runtime_error(
					"Unknown argument '" + argument
						+ "'. Usage: BallCollision [--frame-benchmark [output.csv]]"
				);
			}
		}

		RunBallCollision(config);
		return 0;
	}
	catch (const std::exception& error) {
		std::cerr << "Configuration error: " << error.what() << '\n';
		return 1;
	}
}
