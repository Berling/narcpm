#pragma once

#include <experimental/filesystem>
#include <string>

namespace narcpm {
	struct package {
		enum class state { none, cloned, built };

		std::string name;
		std::string repository;
		bool interface = false;
		std::experimental::filesystem::path location;
		state state = state::none;
	};
}
