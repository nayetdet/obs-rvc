#include <obs-module.h>
#include <plugin-support.h>
#include "rvc/ipc/rvc-ipc.h"
#include "rvc/filter/rvc-filter.h"
#include "rvc/worker/rvc-worker-process.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static rvc_ipc_t *rvc_ipc = NULL;
static rvc_worker_process_t *rvc_worker = NULL;

bool obs_module_load(void)
{
	if (!rvc_worker_start(&rvc_worker))
		return false;

	if (!rvc_ipc_create(&rvc_ipc)) {
		obs_log(LOG_ERROR, "Unable to initialize the OBS RVC iceoryx IPC client");
		rvc_worker_stop(rvc_worker);
		rvc_worker = NULL;
		return false;
	}

	rvc_filter_register(rvc_ipc);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	rvc_filter_shutdown();
	rvc_ipc_destroy(rvc_ipc);
	rvc_ipc = NULL;
	rvc_worker_stop(rvc_worker);
	rvc_worker = NULL;
	obs_log(LOG_INFO, "plugin unloaded");
}
