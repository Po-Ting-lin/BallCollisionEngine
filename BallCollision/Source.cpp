#include "UI_mainConsole.h"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[])
{
	try {
		std::filesystem::path configPath = "config.ini";
		if (!std::filesystem::exists(configPath) && argc > 0) {
			configPath = std::filesystem::absolute(argv[0])
				.parent_path()
				/ "config.ini";
		}

		RunBallCollision(LoadAppConfig(configPath));
		return 0;
	}
	catch (const std::exception& error) {
		std::cerr << "Configuration error: " << error.what() << '\n';
		return 1;
	}
}
