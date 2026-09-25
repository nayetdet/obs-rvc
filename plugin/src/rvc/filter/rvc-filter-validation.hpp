#pragma once

#include <obs-data.h>
#include <obs-properties.h>

#include <string>

namespace rvc::filter {

inline constexpr char kModel[] = "model";
inline constexpr char kHubertPath[] = "hubert_path";
inline constexpr char kRmvpePath[] = "rmvpe_path";
inline constexpr char kModelStatus[] = "model_status";

struct ModelValidation {
	bool valid;
	std::string message;
};

ModelValidation validate_models(obs_data_t *settings);
bool model_path_modified(obs_properties_t *properties, obs_property_t *property, obs_data_t *settings);

} // namespace rvc::filter
