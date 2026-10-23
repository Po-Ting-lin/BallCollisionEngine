#include "UI_mainConsole.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
	bool hasConsole = true;

	// A double-clicked console program gets a console window of its own.
	// Release it so only the game window stays open; keep it when started
	// from a terminal, which shares its console with this process.
	void releaseOwnConsole() {
#ifdef _WIN32
		DWORD processes[2];
		if (GetConsoleProcessList(processes, 2) == 1) {
			FreeConsole();
			hasConsole = false;
		}
#endif
	}

	void reportError(const std::string& message) {
		std::cerr << message << '\n';
#ifdef _WIN32
		if (!hasConsole)
			MessageBoxA(nullptr, message.c_str(), "BallCollision", MB_ICONERROR | MB_OK);
#endif
	}
}

int main(int argc, char* argv[])
{
	try {
		std::filesystem::path configPath = "config.ini";
		if (!std::filesystem::exists(configPath) && argc > 0) {
			configPath = std::filesystem::absolute(argv[0])
				.parent_path()
				/ "config.ini";
		}

		bool frameBenchmark = false;
		std::filesystem::path frameBenchmarkOutput;
		for (int index = 1; index < argc; ++index) {
			const std::string argument = argv[index];
			if (argument == "--frame-benchmark") {
				frameBenchmark = true;
				if (index + 1 < argc
					&& std::string(argv[index + 1]).rfind("--", 0) != 0) {
					frameBenchmarkOutput = argv[++index];
				}
			}
			else {
				throw std::runtime_error(
					"Unknown argument '" + argument
						+ "'. Usage: BallCollision [--frame-benchmark [output.csv]]"
				);
			}
		}

		// the frame benchmark reports to the console
		if (!frameBenchmark)
			releaseOwnConsole();

		AppConfig config = LoadAppConfig(configPath);
		config.frameBenchmark = frameBenchmark;
		config.frameBenchmarkOutput = frameBenchmarkOutput;
		RunBallCollision(config);
		return 0;
	}
	catch (const std::exception& error) {
		reportError(std::string("Configuration error: ") + error.what());
		return 1;
	}
}
