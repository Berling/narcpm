#include <fstream>

#include "config.hpp"
#include "util.hpp"

namespace narcpm {
	config::config(const std::experimental::filesystem::path& config_path) {
		if (!std::experimental::filesystem::exists(config_path) ||
		    !std::experimental::filesystem::is_regular_file(config_path)) {
			throw std::runtime_error{"no config found for package " + config_path.native()};
		}

		std::ifstream config{config_path};
		while (!config.eof()) {
			auto delim = config.peek();
			if (delim == '[') {
				auto section = parse_section(config);
				_sections.insert(std::make_pair(section.name, section));
			} else {
				throw std::runtime_error{"no section found in configuration"};
			}
		}
	}

	config::section config::parse_section(std::ifstream& config) {
		std::string line;
		std::getline(config, line);
		section section;
		section.name = parse_section_head(line);
		while (!config.eof() && config.peek() != '[' && std::getline(config, line)) {
			section.key_value_pairs.insert(parse_key_value_pair(line));
		}
		return section;
	}

	std::string config::parse_section_head(const std::string& line) {
		auto delim = line.find(']');
		if (delim == std::string::npos) {
			throw std::runtime_error{"missing section delimiter"};
		}
		return line.substr(1, delim - 1);
	}

	std::pair<std::string, std::string> config::parse_key_value_pair(const std::string& line) {
		auto delim = line.find('=');
		if (delim == std::string::npos) {
			throw std::runtime_error{"missing key value delimiter"};
		}
		auto key = line.substr(0, delim);
		key = trim(key);
		auto value = line.substr(delim + 1, line.size());
		value = trim(value);
		return {key, value};
	}
}
