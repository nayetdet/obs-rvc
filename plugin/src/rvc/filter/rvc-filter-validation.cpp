#include "rvc-filter-validation.hpp"

#include "utils/file-utils.hpp"

#include <obs.h>

namespace rvc::filter {

ModelValidation validate_models(obs_data_t *settings)
{
	const char *model = obs_data_get_string(settings, kModel);
	const char *hubert = obs_data_get_string(settings, kHubertPath);
	const char *rmvpe = obs_data_get_string(settings, kRmvpePath);
	if (!rvc::utils::is_regular_file(model, ".pth"))
		return {false, "Choose an existing RVC model file (.pth)."};
	if (!rvc::utils::is_regular_file(hubert, ".pt"))
		return {false, "Choose an existing HuBERT model file (.pt)."};
	if (!rvc::utils::is_regular_file(rmvpe, nullptr, "rmvpe.pt"))
		return {false, "Choose the RMVPE model file named rmvpe.pt."};
	return {true, "All model files are valid."};
}

bool model_path_modified(obs_properties_t *properties, obs_property_t *, obs_data_t *settings)
{
	const ModelValidation validation = validate_models(settings);
	obs_property_t *status = obs_properties_get(properties, kModelStatus);
	if (status == nullptr)
		return false;

	obs_property_set_description(status, validation.message.c_str());
	obs_property_text_set_info_type(status, validation.valid ? OBS_TEXT_INFO_NORMAL : OBS_TEXT_INFO_ERROR);
	return true;
}

} // namespace rvc::filter
