#include "narcpm.hpp"

int main(int argc, char** argv) {
	if (argc != 2) {
		throw std::runtime_error{"no directory to root CMakeLists.txt specified"};
	}

	if (!std::experimental::filesystem::exists("narcpm")) {
		throw std::runtime_error{"narcpm can only run from its root directory"};
	}

	narcpm::narcpm narcpm{argv[1]};
	return 0;
}
