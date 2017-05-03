#include <cstdlib>
#include <fstream>
#include <iostream>

#include "config.hpp"
#include "narcpm.hpp"
#include "toolchain.hpp"
#include "util.hpp"

namespace narcpm {
	narcpm::narcpm(const std::experimental::filesystem::path& cmake_lists_location)
	    : _cmake_lists_location{std::experimental::filesystem::canonical(cmake_lists_location)} {
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
		write_dependencies();
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
		std::string package_build_config;
		while (std::getline(config, package_build_config)) {
			auto delimiter = package_build_config.find("::", 0);
			auto package_name = package_build_config.substr(0, delimiter);
			if (!exists(package_name)) {
				continue;
			}
			bool build_static = true;
			if (delimiter != std::string::npos) {
				auto build_type = package_build_config.substr(delimiter + 2, package_build_config.size());
				if (build_type == "static") {
					build_static = true;
				} else if ("shared") {
					build_static = false;
				} else {
					throw std::runtime_error{"unrecognized link type"};
				}
			}
			_build_types[package_name] = build_static;
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
		config config{package_config_location};
		package pack;
		auto package_section_iter = config.find("package");
		if (package_section_iter == config.end()) {
			throw std::runtime_error{"no package section found"};
		}
		auto& package_section = package_section_iter->second;
		pack.name = package_name;
		pack.location = std::experimental::filesystem::path{"cache"} / pack.name;
		for (auto& section_pair : config) {
			if (section_pair.first != "package") {
				auto& section = section_pair.second;
				package subpackage;
				subpackage.name = pack.name + "_" + section.name;
				subpackage.location = pack.location;
				auto interface = section.key_value_pairs.find("interface");
				if (interface != section.key_value_pairs.end()) {
					subpackage.interface = to_bool(interface->second);
				}
				pack.sub_packages[subpackage.name] = std::make_shared<package>(subpackage);
			}
		}
		auto repository = package_section.key_value_pairs.find("repository");
		if (repository != package_section.key_value_pairs.end()) {
			pack.repository = repository->second;
		}
		auto interface = package_section.key_value_pairs.find("interface");
		if (interface != package_section.key_value_pairs.end()) {
			pack.interface = to_bool(interface->second);
		}

		return pack;
	}

	void narcpm::update_repository_cache() {
		std::cout << "-- updating repository cache" << std::endl;
		for (auto& package : _packages) {
			std::experimental::filesystem::path repository_location = package.location / "repository";
			if (package.state == package::state::none && !package.repository.empty()) {
				std::cout << "--   cloning " + package.name << std::endl;
				std::system(
				    ("git clone --depth 1 -q " + package.repository + " " + repository_location.native())
				        .c_str());
			}
		}
	}

	void narcpm::update_build_cache() {
		std::cout << "-- updating build cache" << std::endl;
		std::vector<toolchain> toolchains = {{"clang", "clang", "clang++"}, {"gcc", "gcc", "g++"}};
		std::vector<std::string> build_types = {"Debug", "Release"};
		std::vector<std::string> link_types = {"static", "shared"};
		for (auto& package : _packages) {
			if (package.state == package::state::built) {
				continue;
			}
			std::cout << "--   building package " << package.name << " - ";
			std::experimental::filesystem::path package_root = "cache/" + package.name;
			std::experimental::filesystem::path cmake_lists = "packages/" + package.name;
			bool successfull = false;
			for (auto& toolchain : toolchains) {
				std::experimental::filesystem::path toolchain_location =
				    package_root / "build" / toolchain.name;
				for (auto& build_type : build_types) {
					std::experimental::filesystem::path build_type_location = toolchain_location / build_type;
					std::experimental::filesystem::create_directories(build_type_location);
					auto command =
					    "cd " + build_type_location.native() +
					    " && cmake -G\"Unix Makefiles\" -DCMAKE_BUILD_TYPE=" + build_type +
					    " -DCMAKE_C_COMPILER=" + toolchain.cc + " -DCMAKE_CXX_COMPILER=" + toolchain.cxx +
					    " -DTOOLCHAIN=" + toolchain.name +
					    " -DPACKAGE_ROOT=" + std::experimental::filesystem::canonical(package_root).native() +
					    " " + std::experimental::filesystem::canonical(cmake_lists).native() +
					    " && make -j 8 && make install";
					auto exit_code = std::system(command.c_str());
					if (exit_code != 0) {
						throw std::runtime_error{"could not build package " + package.name};
					} else {
						successfull = true;
					}
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

	void narcpm::write_dependencies() {
		std::cout << "-- writing dependencies - ";
		auto dependency_lists = _cmake_lists_location / "dependencies.cmake";
		std::ofstream lists{dependency_lists, std::ofstream::trunc};
		if (!lists) {
			throw std::runtime_error{"could not open " + dependency_lists.native()};
		}
		lists << "cmake_minimum_required(VERSION 3.7 FATAL_ERROR)" << std::endl;
		lists << std::endl;
		lists << "if(${CMAKE_C_COMPILER_ID} MATCHES \"Clang\" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "
		         "\"Clang\")"
		      << std::endl;
		lists << "\tset(TOOLCHAIN clang)" << std::endl;
		lists << "elseif(${CMAKE_C_COMPILER_ID} MATCHES \"GNU\" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "
		         "\"GNU\")"
		      << std::endl;
		lists << "\tset(TOOLCHAIN gnu)" << std::endl;
		lists << "else()" << std::endl;
		lists << "\tmessage(ERROR \"unrecognized toolchain cc:${CMAKE_C_COMPILER_ID} "
		         "cxx:${CMAKE_CXX_COMPILER_ID}\")"
		      << std::endl;
		lists << "endif()" << std::endl;
		lists << std::endl;
		lists << "if(${CMAKE_BUILD_TYPE} MATCHES \"Debug\")" << std::endl;
		lists << "\tset(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG})" << std::endl;
		lists << "else()" << std::endl;
		lists << "\tset(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})"
		      << std::endl;
		lists << "endif()" << std::endl;

		for (auto& package : _packages) {
			if (package.sub_packages.empty()) {
				if (package.interface) {
					write_interface(lists, package);
				} else {
					write_library(lists, package);
				}
			} else {
				for (auto& sub_package_entry : package.sub_packages) {
					auto& sub_package = *sub_package_entry.second;
					if (sub_package.interface) {
						write_interface(lists, sub_package);
					} else {
						write_library(lists, sub_package);
					}
				}
			}
		}
		std::cout << "done" << std::endl;
	}

	void narcpm::write_interface(std::ofstream& lists, const package& package) {
		lists << std::endl;
		lists << "add_library(" << package.name << " INTERFACE IMPORTED)" << std::endl;
		lists << "set_property(TARGET " << package.name << " PROPERTY" << std::endl;
		auto package_include_dir =
		    std::experimental::filesystem::canonical(package.location / "include");
		lists << "\tINTERFACE_INCLUDE_DIRECTORIES " << package_include_dir.native() << ")" << std::endl;
	}

	void narcpm::write_library(std::ofstream& lists, const package& package) {
		lists << std::endl;
		lists << "add_library(" << package.name << " " << library_type(package) << " IMPORTED)"
		      << std::endl;
		lists << "set_property(TARGET " << package.name << " PROPERTY" << std::endl;
		auto package_include_dir =
		    std::experimental::filesystem::canonical(package.location / "include");
		lists << "\tINTERFACE_INCLUDE_DIRECTORIES " << package_include_dir.native() << ")" << std::endl;
		lists << "set_property(TARGET " << package.name << " PROPERTY" << std::endl;
		lists << "\tIMPORTED_LOCATION " << generate_library_path(package) << ")" << std::endl;
		if (!_build_types[package.name]) {
			lists << "execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "
			      << std::experimental::filesystem::canonical(package.location) / "lib" / "${TOOLCHAIN}" /
			             "${CMAKE_BUILD_TYPE}" / generate_library_name(package)
			      << " ${RUNTIME_OUTPUT_DIRECTORY}/" + generate_library_name(package) << ")" << std::endl;
		}
	}

	std::string narcpm::generate_library_name(const package& package) {
		auto build_static = _build_types[package.name];
		auto library_name = "lib" + package.name;
		if (build_static) {
			library_name += ".a";
		} else {
			library_name += ".so";
		}
		return library_name;
	}

	std::string narcpm::library_type(const package& package) {
		auto build_static = _build_types[package.name];
		if (build_static) {
			return "STATIC";
		} else {
			return "SHARED";
		}
	}

	std::string narcpm::generate_library_path(const package& package) {
		auto build_static = _build_types[package.name];
		auto lib_name = generate_library_name(package);
		if (build_static) {
			return std::experimental::filesystem::canonical(package.location) / "lib" / "${TOOLCHAIN}" /
			       "${CMAKE_BUILD_TYPE}" / generate_library_name(package);

		} else {
			return "${RUNTIME_OUTPUT_DIRECTORY}/" + lib_name;
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
