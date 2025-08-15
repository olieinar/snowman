#include <string>
namespace snowboy {
	// satisfies unresolved external coming from snowboy-io.obj
	std::string CharToString(char const& c) {
		return std::string(1, c);
	}
} // namespace snowboy
