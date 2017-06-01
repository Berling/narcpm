#include "narcpm.hpp"

int main(int argc, char** argv) {
	if (argc != 3) {
		throw std::runtime_error{"no directory to root CMakeLists.txt specified"};
	}

	if (!std::experimental::filesystem::exists("narcpm")) {
		throw std::runtime_error{"narcpm can only run from its root directory"};
	}

	narcpm::narcpm::mode mode;
	std::string mode_string{argv[1]};
	if (mode_string == "--init") {
		mode = narcpm::narcpm::mode::init;
	} else if (mode_string == "--run") {
		mode = narcpm::narcpm::mode::run;
	} else {
		throw std::runtime_error{"unrecognized mode"};
	}

	narcpm::narcpm narcpm{argv[2], mode};
	return 0;
}
