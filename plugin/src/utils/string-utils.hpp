#pragma once

#include <cstddef>
#include <cstring>

namespace rvc::utils {

template<size_t Size> bool copy_string(char (&destination)[Size], const char *source)
{
	if (source == nullptr)
		source = "";

	const size_t length = std::strlen(source);
	if (length >= Size)
		return false;

	std::memcpy(destination, source, length + 1U);
	return true;
}

} // namespace rvc::utils
