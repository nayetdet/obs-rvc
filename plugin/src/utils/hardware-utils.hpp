#pragma once

#include <graphics/graphics.h>
#include <obs.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#if defined(__linux__)
#include <sched.h>
#endif

namespace rvc::utils {
inline std::string software_rendering_warning()
{
	obs_enter_graphics();
	const char *device_name = gs_get_device_name();
	const std::string device = device_name != nullptr ? device_name : "";
	obs_leave_graphics();

	std::string normalized_device = device;
	std::transform(normalized_device.begin(), normalized_device.end(), normalized_device.begin(),
		       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	const bool software_renderer = normalized_device.find("llvmpipe") != std::string::npos ||
				       normalized_device.find("softpipe") != std::string::npos ||
				       normalized_device.find("swiftshader") != std::string::npos ||
				       normalized_device.find("basic render driver") != std::string::npos ||
				       normalized_device.find("microsoft warp") != std::string::npos;
	bool has_render_device = true;
#if defined(__linux__)
	has_render_device = false;
	std::error_code error;
	for (std::filesystem::directory_iterator entry("/dev/dri", error);
	     !error && entry != std::filesystem::directory_iterator(); entry.increment(error)) {
		if (entry->path().filename().string().rfind("renderD", 0) == 0) {
			has_render_device = true;
			break;
		}
	}
#endif
	if (!software_renderer && has_render_device)
		return {};

	const std::string renderer = device.empty() ? "unknown renderer" : device;
	return "OBS is rendering on the CPU (" + renderer +
	       "). This competes with RVC inference and can cause audio underflows. Enable GPU rendering.";
}

inline int64_t available_cpu_count()
{
	const uint32_t hardware_threads = std::thread::hardware_concurrency();
	int64_t available = hardware_threads == 0U ? 1 : static_cast<int64_t>(hardware_threads);
#if defined(__linux__)
	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	if (sched_getaffinity(0, sizeof(affinity), &affinity) == 0) {
		const int affinity_threads = CPU_COUNT(&affinity);
		if (affinity_threads > 0)
			available = std::min<int64_t>(available, affinity_threads);
	}

	std::ifstream cgroup_v2("/sys/fs/cgroup/cpu.max");
	std::string quota_text;
	int64_t period = 0;
	if (cgroup_v2 >> quota_text >> period && quota_text != "max" && period > 0) {
		std::istringstream quota_stream(quota_text);
		int64_t quota = 0;
		if (quota_stream >> quota && quota > 0)
			available = std::min(available, std::max<int64_t>(1, (quota + period - 1) / period));
	} else {
		std::ifstream quota_file("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
		std::ifstream period_file("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
		int64_t quota = 0;
		if (quota_file >> quota && period_file >> period && quota > 0 && period > 0)
			available = std::min(available, std::max<int64_t>(1, (quota + period - 1) / period));
	}
#endif
	return std::clamp<int64_t>(available, 1, 256);
}

}
