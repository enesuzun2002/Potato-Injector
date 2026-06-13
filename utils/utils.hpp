#pragma once
#include <string>
#include <algorithm>
#include <cwctype>
#include <cctype>
#include <fstream>
#include <vector>
#include <filesystem>

namespace string
{
	inline std::wstring toLower(std::wstring s) {
		std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) {
			return std::towlower(c);
		});
		return s;
	}

	inline std::string toLower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](char c) {
			return std::tolower(static_cast<unsigned char>(c));
		});
		return s;
	}

	template<typename T>
	inline std::string toHex(T val) {
		char buf[32];
		sprintf_s(buf, "0x%llX", static_cast<unsigned long long>(val));
		return std::string(buf);
	}

	template<typename ... arg>
	static std::wstring format(std::wstring_view  fmt, arg ... args) {
		const int size = std::swprintf(nullptr, NULL, fmt.data(), args ...) + 1;
		const auto buf = std::make_unique<wchar_t[]>(size);
		std::swprintf(buf.get(), size, fmt.data(), args ...);

		return std::wstring(buf.get(), buf.get() + size - 1);
	}
}

namespace utils
{

	inline bool readFileToMem(const std::filesystem::path& path, std::vector<BYTE>& buffer) {
		std::ifstream file(path, std::ios::binary);
		if (file.fail()) return false;

		buffer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		file.close();

		return true;
	}
}