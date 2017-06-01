#include <cstdlib>
#include <fstream>
#include <iostream>

#include "config.hpp"
#include "narcpm.hpp"
#include "toolchain.hpp"
#include "util.hpp"

namespace narcpm {
	narcpm::narcpm(const std::experimental::filesystem::path& cmake_lists_location, mode mode)
	    : _cmake_lists_location{std::experimental::filesystem::canonical(cmake_lists_location)},
	      _mode{mode} {
		if (!std::experimental::filesystem::exists(_cmake_lists_location) ||
		    !std::experimental::filesystem::is_directory(_cmake_lists_location)) {
			throw std::runtime_error{_cmake_lists_location.native() + " no such directory"};
		}

		_narcpm_location = std::experimental::filesystem::canonical(_narcpm_location);

		if (!std::experimental::filesystem::exists("packages") ||
		    !std::experimental::filesystem::is_directory("packages")) {
			std::cout << "-- cloning package definition" << std::endl;
			std::system("git clone -q https://github.com/Berling/narcpm_packages.git packages");
		} else {
			std::cout << "-- updating package definition" << std::endl;
			std::system("cd packages && git pull -q");
		}

		if (mode == mode::init) {
			configure_project();
		} else if (mode == mode::run) {
			find_imports();
			find_packages();
			update_repository_cache();
			update_build_cache();
			write_dependencies();
		}
	}

	void narcpm::configure_project() {
		auto narcpm_cmake_location = _cmake_lists_location / "narcpm.cmake";
		std::ofstream narcpm_cmake{narcpm_cmake_location, std::ofstream::trunc};
		if (!narcpm_cmake) {
			throw std::runtime_error{"could not open " + narcpm_cmake_location.native()};
		}
		std::experimental::filesystem::path narcpm_location{"narcpm"};
		narcpm_location = std::experimental::filesystem::canonical(narcpm_location);
		narcpm_cmake << "execute_process(COMMAND ${CMAKE_COMMAND} -E chdir "
		             << narcpm_location.parent_path().native() << " " << narcpm_location.native()
		             << " --run " << _cmake_lists_location.native() << ")" << std::endl;
		auto clang_complete_location = _cmake_lists_location / ".clang-complete";
	}

	void narcpm::find_imports() {
		std::experimental::filesystem::path config_location = _cmake_lists_location / "narcpm.config";
		config import_config{config_location};
		for (auto& import_config_entry : import_config) {
			auto& import_config = import_config_entry.second;
			import import;
			import.name = import_config_entry.first;
			auto link_static_iter = import_config.key_value_pairs.find("static");
			if (link_static_iter != import_config.key_value_pairs.end()) {
				import.link_static = to_bool(link_static_iter->second);
			}
			auto update_iter = import_config.key_value_pairs.find("update");
			if (update_iter != import_config.key_value_pairs.end()) {
				import.update = to_bool(update_iter->second);
			}
			for (auto& subpackage_config_entry : import_config.subsections) {
				auto& subpackage_config = *subpackage_config_entry.second;
				struct import subimport;
				subimport.name = subpackage_config_entry.first;
				auto link_static_iter = subpackage_config.key_value_pairs.find("static");
				if (link_static_iter != import_config.key_value_pairs.end()) {
					subimport.link_static = to_bool(link_static_iter->second);
				}
				import.subpackages.emplace_back(subimport);
			}
			_imports.emplace_back(import);
		}
	}

	void narcpm::find_packages() {
		for (auto& import : _imports) {
			package package = find_package_config(import.name);
			if (!exists(package.name)) {
				continue;
			}
			if (cloned(import.name)) {
				package.state = package::state::cloned;
			}
			if (built(import.name)) {
				package.state = package::state::built;
			}
			_packages[package.name] = package;
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
		pack.location = _narcpm_location / std::experimental::filesystem::path{"cache"} / pack.name;
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
				pack.sub_packages[section.name] = std::make_shared<package>(subpackage);
			}
		}
		auto repository = package_section.key_value_pairs.find("repository");
		if (repository != package_section.key_value_pairs.end()) {
			pack.repository = repository->second;
		}
		auto commit = package_section.key_value_pairs.find("commit");
		if (commit != package_section.key_value_pairs.end()) {
			pack.commit = commit->second;
		}
		auto interface = package_section.key_value_pairs.find("interface");
		if (interface != package_section.key_value_pairs.end()) {
			pack.interface = to_bool(interface->second);
		}

		return pack;
	}

	void narcpm::update_repository_cache() {
		std::cout << "-- updating repository cache" << std::endl;
		for (auto& package_entry : _packages) {
			auto& package = package_entry.second;
			std::experimental::filesystem::path repository_location = package.location / "repository";
			if (package.state == package::state::none && !package.repository.empty()) {
				std::cout << "--   cloning " << package.name << std::endl;
				std::string depth;
				std::string checkout;
				if (!package.commit.empty()) {
					checkout =
					    " && cd " + repository_location.native() + " && git checkout " + package.commit;
				} else {
					depth = "--depth 1";
				}
				std::string clone = "git clone " + depth + " -q " + package.repository + " " +
				                    repository_location.native() + checkout;
				std::system(clone.c_str());
			} else if (package.state != package::state::none && !package.repository.empty()) {
				std::cout << "--   update " << package.name << std::endl;
				std::string update = "cd " + repository_location.native() + " && git pull";
				auto result = std::system(update.c_str());
				if (result) {
					std::experimental::filesystem::remove(package.location / "build");
				}
			}
		}
	}

	void narcpm::update_build_cache() {
		std::cout << "-- updating build cache" << std::endl;
		std::vector<toolchain> toolchains = {{"clang", "clang", "clang++"}, {"gcc", "gcc", "g++"}};
		std::vector<std::string> build_types = {"Debug", "Release"};
		std::vector<std::string> link_types = {"static", "shared"};
		for (auto& package_entry : _packages) {
			auto& package = package_entry.second;
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
		lists << "\tset(TOOLCHAIN gcc)" << std::endl;
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

		for (auto& import : _imports) {
			auto& package = _packages[import.name];
			if (import.subpackages.empty()) {
				if (package.interface) {
					write_interface(lists, package);
				} else {
					write_library(lists, import, package);
				}
			} else {
				for (auto& subimport : import.subpackages) {
					if (!package.sub_packages[subimport.name]) {
						throw std::runtime_error{"could not find subpackage " + import.name + " " +
						                         subimport.name};
					}
					auto& subpackage = *package.sub_packages[subimport.name];
					if (subpackage.interface) {
						write_interface(lists, subpackage);
					} else {
						write_library(lists, subimport, subpackage);
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

	void narcpm::write_library(std::ofstream& lists, const import& import, const package& package) {
		lists << std::endl;
		lists << "add_library(" << package.name << " " << library_type(import) << " IMPORTED)"
		      << std::endl;
		lists << "set_property(TARGET " << package.name << " PROPERTY" << std::endl;
		auto package_include_dir =
		    std::experimental::filesystem::canonical(package.location / "include");
		lists << "\tINTERFACE_INCLUDE_DIRECTORIES " << package_include_dir.native() << ")" << std::endl;
		lists << "set_property(TARGET " << package.name << " PROPERTY" << std::endl;
		lists << "\tIMPORTED_LOCATION " << generate_library_path(import, package) << ")" << std::endl;
		if (!import.link_static) {
			lists << "execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "
			      << std::experimental::filesystem::canonical(package.location) / "lib" / "${TOOLCHAIN}" /
			             "${CMAKE_BUILD_TYPE}" / generate_library_name(import, package)
			      << " ${RUNTIME_OUTPUT_DIRECTORY}/" + generate_library_name(import, package) << ")"
			      << std::endl;
		}
	}

	std::string narcpm::generate_library_name(const import& import, const package& package) {
		auto library_name = "lib" + package.name;
		if (import.link_static) {
			library_name += ".a";
		} else {
			library_name += ".so";
		}
		return library_name;
	}

	std::string narcpm::library_type(const import& import) {
		if (import.link_static) {
			return "STATIC";
		} else {
			return "SHARED";
		}
	}

	std::string narcpm::generate_library_path(const import& import, const package& package) {
		auto lib_name = generate_library_name(import, package);
		if (import.link_static) {
			return std::experimental::filesystem::canonical(package.location) / "lib" / "${TOOLCHAIN}" /
			       "${CMAKE_BUILD_TYPE}" / generate_library_name(import, package);

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
