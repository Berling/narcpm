#pragma once

#include <experimental/filesystem>
#include <string>
#include <unordered_map>

namespace narcpm {
	class config {
	private:
		struct section {
			std::string name;
			std::unordered_map<std::string, std::string> key_value_pairs;
		};

		std::unordered_map<std::string, section> _sections;

	public:
		config(const std::experimental::filesystem::path& config_path);

		auto find(const std::string& section) {
			return _sections.find(section);
		}

		auto begin() {
			return _sections.begin();
		}

		auto begin() const {
			return _sections.begin();
		}

		auto end() {
			return _sections.end();
		}

		auto end() const {
			return _sections.end();
		}

	private:
		section parse_section(std::ifstream& config);
		std::string parse_section_head(const std::string& line);
		std::pair<std::string, std::string> parse_key_value_pair(const std::string& line);
	};
}
