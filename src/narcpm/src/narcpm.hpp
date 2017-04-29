#pragma once

#include <experimental/filesystem>
#include <unordered_map>
#include <vector>

#include "package.hpp"

namespace narcpm {
	class narcpm {
	private:
		std::vector<package> _packages;
		std::experimental::filesystem::path _cmake_lists_location;
		std::unordered_map<std::string, bool> _build_types;

	public:
		narcpm(const std::experimental::filesystem::path& cmake_lists_location);

	private:
		void find_packages();
		package find_package_config(const std::string& package_name);
		void update_repository_cache();
		void update_build_cache();
		void write_dependencies();
		void write_interface(std::ofstream& lists, const package& package);
		void write_library(std::ofstream& lists, const package& package);
		std::string generate_library_name(const package& package);
		bool exists(const std::string& package_name);
		bool cloned(const std::string& package_name);
		bool built(const std::string& package_name);
	};
}
