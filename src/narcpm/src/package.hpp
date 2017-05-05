#pragma once

#include <experimental/filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace narcpm {
	struct package {
		enum class state { none, cloned, built };

		std::string name;
		std::string repository;
		std::string commit;
		bool interface = false;
		std::experimental::filesystem::path location;
		state state = state::none;
		std::unordered_map<std::string, std::shared_ptr<package>> sub_packages;
	};

	struct import {
		std::string name;
		bool link_static = true;
		std::vector<import> subpackages;
	};
}
