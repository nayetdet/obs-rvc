#pragma once

#include <graphics/graphics.h>
#include <obs.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
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

inline int64_t physical_cpu_count()
{
#if defined(__linux__)
	std::set<std::pair<std::string, std::string>> physical_cores;
	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	const bool has_affinity = sched_getaffinity(0, sizeof(affinity), &affinity) == 0;
	const std::filesystem::path cpu_root("/sys/devices/system/cpu");
	std::error_code error;
	for (auto entry = std::filesystem::directory_iterator(cpu_root, error);
	     !error && entry != std::filesystem::directory_iterator(); entry.increment(error)) {
		const std::string name = entry->path().filename().string();
		if (name.rfind("cpu", 0) != 0 || name.size() <= 3 ||
		    !std::all_of(name.begin() + 3, name.end(), [](unsigned char value) { return std::isdigit(value); }))
			continue;

		const unsigned long cpu = std::stoul(name.substr(3));
		if (has_affinity && (cpu >= CPU_SETSIZE || !CPU_ISSET(static_cast<int>(cpu), &affinity)))
			continue;

		std::ifstream package_file(entry->path() / "topology/physical_package_id");
		std::ifstream core_file(entry->path() / "topology/core_id");
		std::string package, core;
		if (package_file >> package && core_file >> core)
			physical_cores.emplace(package, core);
	}

	if (!physical_cores.empty())
		return std::clamp<int64_t>(static_cast<int64_t>(physical_cores.size()), 1, 256);
#endif
	const uint32_t detected = std::thread::hardware_concurrency();
	return std::clamp<int64_t>(detected == 0U ? 1U : detected, 1, 256);
}

}
