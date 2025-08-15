#include <string>

namespace snowboy {
	std::string CharToString(char const& c) {
		return std::string(1, c);
	}
} // namespace snowboy