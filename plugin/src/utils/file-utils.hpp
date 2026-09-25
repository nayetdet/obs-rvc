#pragma once

#include <obs-data.h>
#include <obs-module.h>

#include <filesystem>

namespace rvc::utils {

inline void set_default_module_file(obs_data_t *settings, const char *key, const char *relative_path)
{
	char *path = obs_module_file(relative_path);
	if (path == nullptr)
		return;
	obs_data_set_default_string(settings, key, path);
	bfree(path);
}

inline bool is_regular_file(const char *path, const char *extension = nullptr, const char *filename = nullptr)
{
	if (path == nullptr || path[0] == '\0')
		return false;

	try {
		const std::filesystem::path file = std::filesystem::u8path(path);
		std::error_code error;
		if (!std::filesystem::is_regular_file(file, error) || error)
			return false;
		return (extension == nullptr || file.extension() == extension) &&
		       (filename == nullptr || file.filename() == filename);
	} catch (...) {
		return false;
	}
}

} // namespace rvc::utils
