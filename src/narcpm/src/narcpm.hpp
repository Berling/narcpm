#pragma once

#include <experimental/filesystem>
#include <unordered_map>
#include <vector>

#include "package.hpp"

namespace narcpm {
	class narcpm {
	public:
		enum class mode { init, run };

	private:
		std::vector<import> _imports;
		std::unordered_map<std::string, package> _packages;
		std::experimental::filesystem::path _narcpm_location;
		std::experimental::filesystem::path _cmake_lists_location;
		mode _mode;

	public:
		narcpm(const std::experimental::filesystem::path& cmake_lists_location, mode mode);

	private:
		void configure_project();
		void find_imports();
		void find_packages();
		package find_package_config(const std::string& package_name);
		void update_repository_cache();
		void update_build_cache();
		void write_dependencies();
		void write_interface(std::ofstream& lists, const package& package);
		void write_library(std::ofstream& lists, const import& import, const package& package);
		std::string generate_library_name(const import& import, const package& package);
		std::string library_type(const import& import);
		std::string generate_library_path(const import& import, const package& package);
		bool exists(const std::string& package_name);
		bool cloned(const std::string& package_name);
		bool built(const std::string& package_name);
	};
}
