#include <cstdlib>
#include <fstream>
#include <iostream>

#include "narcpm.hpp"
#include "toolchain.hpp"

namespace narcpm {
	narcpm::narcpm(const std::experimental::filesystem::path& cmake_lists_location)
	    : _cmake_lists_location{cmake_lists_location} {
		if (!std::experimental::filesystem::exists(_cmake_lists_location) ||
		    !std::experimental::filesystem::is_directory(_cmake_lists_location)) {
			throw std::runtime_error{_cmake_lists_location.native() + " no such directory"};
		}

		if (!std::experimental::filesystem::exists("packages") ||
		    !std::experimental::filesystem::is_directory("packages")) {
			std::cout << "-- cloning package definition" << std::endl;
			std::system("git clone -q https://github.com/Berling/narcpm_packages.git packages");
		} else {
			std::cout << "-- updating package definition" << std::endl;
			std::system("cd packages && git pull -q");
		}

		find_packages();
		update_repository_cache();
		update_build_cache();
	}

	void narcpm::find_packages() {
		std::experimental::filesystem::path config_location = _cmake_lists_location / "narcpm.config";
		if (!std::experimental::filesystem::exists(config_location)) {
			throw std::runtime_error{"no narcpm.config found in " + _cmake_lists_location.native()};
		}
		std::ifstream config{config_location};
		if (!config) {
			throw std::runtime_error{"could not open narcpm.config"};
		}
		std::string package_name;
		while (std::getline(config, package_name)) {
			if (!exists(package_name)) {
				continue;
			}
			package package = find_package_config(package_name);
			package.name = package_name;
			package.location = "cache/" + package_name;
			if (cloned(package_name)) {
				package.state = package::state::cloned;
			}
			if (built(package_name)) {
				package.state = package::state::built;
			}
			_packages.emplace_back(package);
		}
	}

	package narcpm::find_package_config(const std::string& package_name) {
		std::experimental::filesystem::path package_config_location{"packages/" + package_name + "/" +
		                                                            package_name + ".config"};
		if (!std::experimental::filesystem::exists(package_config_location) ||
		    !std::experimental::filesystem::is_regular_file(package_config_location)) {
			throw std::runtime_error{"no config found for package " + package_name};
		}
		std::ifstream config{package_config_location};
		if (!config) {
			throw std::runtime_error{"could not open " + package_config_location.native()};
		}
		std::string repository;
		if (!std::getline(config, repository)) {
			throw std::runtime_error{package_name + ".config is empty"};
		}
		package package;
		std::string interface;
		if (!std::getline(config, interface)) {
			throw std::runtime_error{package_name + ".config has no interface flag"};
		}
		package.repository = repository;
		if (interface == "true") {
			package.interface = true;
		} else if (interface == "false") {
			package.interface = false;
		} else {
			throw std::runtime_error{package_name + ".config has illegal value for interface flag"};
		}
		return package;
	}

	void narcpm::update_repository_cache() {
		std::cout << "-- updating repository cache" << std::endl;
		for (auto& package : _packages) {
			std::experimental::filesystem::path repository_location = package.location / "repository";
			if (package.state == package::state::none) {
				std::cout << "--   cloning " + package.name << std::endl;
				std::system(
				    ("git clone --depth 1 -q " + package.repository + " " + repository_location.native())
				        .c_str());
			}
		}
	}

	void narcpm::update_build_cache() {
		std::cout << "-- updating build cache" << std::endl;
		std::vector<toolchain> toolchains = {{"clang", "clang", "clang++"}, {"gnu", "gcc", "g++"}};
		std::vector<std::string> build_types = {"Debug", "Release"};
		std::vector<std::string> link_types = {"static", "shared"};
		for (auto& package : _packages) {
			if (package.state == package::state::built) {
				continue;
			}
			std::cout << "--   building package " << package.name << " - ";
			std::experimental::filesystem::path package_root = "cache/" + package.name;
			bool successfull = false;
			if (!package.interface) {
				for (auto& toolchain : toolchains) {
					std::experimental::filesystem::path toolchain_location =
					    package_root / "build" / toolchain.name;
					for (auto& build_type : build_types) {
						std::experimental::filesystem::path build_type_location =
						    toolchain_location / build_type;
						for (auto& link_type : link_types) {
							std::experimental::filesystem::path link_type_location =
							    build_type_location / link_type;
							auto exit_code = std::system("false");
							if (exit_code != 0) {
								successfull = false;
							} else {
								successfull = true;
							}
						}
					}
				}
			} else {
				std::experimental::filesystem::path cmake_lists = "packages/" + package.name;
				std::experimental::filesystem::path build_location = package_root / "build";
				std::experimental::filesystem::create_directories(build_location);
				auto exit_code = std::system(
				    ("cd " + build_location.native() + " && cmake -G\"Unix Makefiles\" -DPACKAGE_ROOT=" +
				     std::experimental::filesystem::canonical(package_root).native() + " " +
				     std::experimental::filesystem::canonical(cmake_lists).native() +
				     " && make install -j 8")
				        .c_str());
				if (exit_code != 0) {
					successfull = false;
				} else {
					successfull = true;
				}
			}
			if (successfull) {
				std::cout << "done" << std::endl;
				std::experimental::filesystem::path built_tag_location = package_root / ".built";
				std::ofstream built_tag{built_tag_location};
			} else {
				std::cout << "failed" << std::endl;
			}
		}
	}

	bool narcpm::exists(const std::string& package_name) {
		std::cout << "-- looking for package: " << package_name << " - ";
		std::experimental::filesystem::path package_definition_location{"packages/" + package_name};
		if (!std::experimental::filesystem::exists(package_definition_location) ||
		    !std::experimental::filesystem::is_directory(package_definition_location)) {
			std::cout << "not found" << std::endl;
			return false;
		}
		std::cout << "found" << std::endl;
		return true;
	}

	bool narcpm::cloned(const std::string& package_name) {
		std::cout << "--   checking if package has been cloned - ";
		std::experimental::filesystem::path cache_location = "cache/" + package_name;
		if (!std::experimental::filesystem::exists(cache_location) ||
		    !std::experimental::filesystem::is_directory(cache_location)) {
			std::cout << "no" << std::endl;
			return false;
		}
		std::cout << "yes" << std::endl;
		return true;
	}

	bool narcpm::built(const std::string& package_name) {
		std::cout << "--   checking if package has been built - ";
		if (!std::experimental::filesystem::exists("cache/" + package_name + "/.built")) {
			std::cout << "no" << std::endl;
			return false;
		}
		std::cout << "yes" << std::endl;
		return true;
	}
}
