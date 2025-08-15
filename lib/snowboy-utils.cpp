#include <snowboy-error.h>
#include <snowboy-utils.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <errno.h>
#include <malloc.h> // _aligned_malloc/_aligned_free
#endif

namespace snowboy {

	const std::string global_snowboy_whitespace_set{" \t\n\r\f\v"};

	std::string Basename(const std::string& file) {
		// Handle both Unix and Windows separators
		auto pos = file.find_last_of("/\\");
		if (pos == std::string::npos) return file;
		return file.substr(pos + 1);
	}

	std::string CharToString(char c) {
		std::string res;
		res.resize(16);
		const char* fmt = "\'%c\'";
		if (!std::isprint(static_cast<unsigned char>(c))) fmt = "[character %d]";
		auto len = std::snprintf(res.data(), res.size(), fmt, c);
		if (len < 0) return "";
		res.resize(static_cast<size_t>(len));
		return res;
	}

	bool ConvertStringToBoolean(const std::string& val) {
		return ConvertStringTo<bool>(val);
	}

	template <>
	bool ConvertStringTo<bool>(const std::string& val) {
		auto v = val;
		Trim(&v);
		if (v == "true") return true;
		if (v == "false") return false;
		throw snowboy_exception(std::string{"ConvertStringTo<bool>: Bad value for boolean type: "} + val);
	}

	template <>
	float ConvertStringTo<float>(const std::string& val) {
		auto v = val;
		Trim(&v);
		try {
			return std::stof(v, nullptr);
		} catch (const std::exception&) {
			throw snowboy_exception(std::string{"ConvertStringTo<float>: Bad value for float type: "} + val);
		}
	}

	template <>
	int32_t ConvertStringTo<int32_t>(const std::string& val) {
		auto v = val;
		Trim(&v);
		try {
			return std::stoi(v, nullptr);
		} catch (const std::exception&) {
			throw snowboy_exception(std::string{"ConvertStringTo<int32_t>: Bad value for int32_t type: "} + val);
		}
	}

	template <>
	uint32_t ConvertStringTo<uint32_t>(const std::string& val) {
		auto v = val;
		Trim(&v);
		try {
			return static_cast<uint32_t>(std::stoul(v, nullptr));
		} catch (const std::exception&) {
			throw snowboy_exception(std::string{"ConvertStringTo<uint32_t>: Bad value for uint32_t type: "} + val);
		}
	}

	void FilterConfigString(bool invert, const std::string& prefix, std::string* config_str) {
		if (!prefix.empty()) {
			std::vector<std::string> parts;
			SplitStringToVector(*config_str, global_snowboy_whitespace_set, &parts);
			config_str->clear();
			for (auto& e : parts) {
				auto pos = e.find(prefix, 0);
				if ((pos != std::string::npos && !invert) || (pos == std::string::npos && invert)) {
					config_str->append(" ");
					config_str->append(e);
				}
			}
		}
	}

	void* SnowboyMemalign(size_t align, size_t size) {
#if defined(_MSC_VER)
		// MSVC doesn't have posix_memalign
		return _aligned_malloc(size, align);
#else
		void* ptr = nullptr;
		if (posix_memalign(&ptr, align, size) == 0)
			return ptr;
		else
			return nullptr;
#endif
	}

	void SnowboyMemalignFree(void* ptr) {
#if defined(_MSC_VER)
		_aligned_free(ptr);
#else
		free(ptr);
#endif
	}

	template <>
	void SplitStringToIntegers(const std::string& s1, const std::string& s2, std::vector<int>* out) {
		std::vector<std::string> parts;
		SplitStringToVector(s1, s2, &parts);
		out->resize(parts.size());
		for (size_t i = 0; i < parts.size(); i++) {
			out->at(i) = ConvertStringToIntegerOrFloat<int>(parts[i]);
		}
	}

	template <>
	void SplitStringToIntegers(const std::string& s1, const char* s2, std::vector<int>* out) {
		SplitStringToIntegers(s1, std::string{s2}, out);
	}

	void SplitStringToFloats(const std::string& s1, const std::string& s2, std::vector<float>* out) {
		std::vector<std::string> parts;
		SplitStringToVector(s1, s2, &parts);
		out->resize(parts.size());
		for (size_t i = 0; i < parts.size(); i++) {
			out->at(i) = ConvertStringTo<float>(parts[i]);
		}
	}

	void SplitStringToFloats(const std::string& s1, const char* s2, std::vector<float>* out) {
		std::string stemp{s2};
		SplitStringToFloats(s1, stemp, out);
	}

	void SplitStringToVector(const std::string& s1, const std::string& s2, std::vector<std::string>* out) {
		size_t pos = 0;
		size_t offset = 0;
		do {
			pos = s1.find_first_of(s2, offset);
			std::string str;
			if (pos == std::string::npos) {
				str = s1.substr(offset);
			} else {
				str = s1.substr(offset, pos - offset);
			}
			if (!str.empty())
				out->push_back(std::move(str));
			offset = (pos == std::string::npos) ? pos : pos + 1;
		} while (pos != std::string::npos);
	}

	void SplitStringToVector(const std::string& s1, const char* s2, std::vector<std::string>* out) {
		std::string stemp{s2};
		return SplitStringToVector(s1, stemp, out);
	}

	void Trim(std::string* str) {
		TrimLeft(str);
		TrimRight(str);
	}

	void TrimLeft(std::string* str) {
		auto pos = str->find_first_not_of(global_snowboy_whitespace_set);
		if (pos == std::string::npos) {
			str->clear();
		} else if (pos > 0) {
			str->erase(0, pos);
		}
	}

	void TrimRight(std::string* str) {
		auto pos = str->find_last_not_of(global_snowboy_whitespace_set);
		if (pos != std::string::npos)
			str->resize(pos + 1);
	}

} // namespace snowboy
