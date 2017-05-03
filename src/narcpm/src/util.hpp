#pragma once

#include <algorithm>
#include <sstream>
#include <string>

namespace narcpm {
	static inline auto& ltrim(std::string& string) {
		string.erase(string.begin(), std::find_if(string.begin(), string.end(),
		                                          [](auto c) { return !std::isspace(c); }));
		return string;
	}

	static inline auto& rtrim(std::string& string) {
		string.erase(
		    std::find_if(string.rbegin(), string.rend(), [](auto c) { return !std::isspace(c); })
		        .base(),
		    string.end());
		return string;
	}

	static inline auto& trim(std::string& string) {
		return ltrim(rtrim(string));
	}

	static inline auto to_bool(const std::string& string) {
		std::istringstream iss{string};
		auto value = false;
		iss >> std::boolalpha >> value;
		return value;
	}
}
