#pragma once

#include <experimental/filesystem>
#include <vector>

#include "package.hpp"

namespace narcpm {
	class narcpm {
	private:
		std::vector<package> _packages;
		std::experimental::filesystem::path _cmake_lists_location;

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
		bool exists(const std::string& package_name);
		bool cloned(const std::string& package_name);
		bool built(const std::string& package_name);
	};
}
