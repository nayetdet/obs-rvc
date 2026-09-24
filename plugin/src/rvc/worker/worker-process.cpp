#include "worker-process.h"

#include <obs-module.h>
#include <plugin-support.h>

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

struct rvc_worker_process {
#ifdef _WIN32
	PROCESS_INFORMATION process{};
#else
	pid_t pid = -1;
#endif
};

namespace {
namespace fs = std::filesystem;

#ifdef _WIN32
bool launch(rvc_worker_process_t &worker, const fs::path &path)
{
	std::string command = "\"" + path.string() + "\"";
	STARTUPINFOA startup{};
	startup.cb = sizeof(startup);
	return CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
			      path.parent_path().string().c_str(), &startup, &worker.process) != FALSE;
}
#else
bool launch(rvc_worker_process_t &worker, const fs::path &path)
{
	worker.pid = fork();
	if (worker.pid < 0)
		return false;
	if (worker.pid == 0) {
		if (chdir(path.parent_path().c_str()) != 0)
			_exit(127);
		execl(path.c_str(), path.c_str(), nullptr);
		_exit(127);
	}
	return true;
}
#endif
} // namespace

bool rvc_worker_start(rvc_worker_process_t **process)
{
	if (process == nullptr)
		return false;

#ifdef _WIN32
	constexpr const char *worker_name = "worker/obs-rvc-worker.exe";
#else
	constexpr const char *worker_name = "worker/obs-rvc-worker";
#endif
	char *worker_file = obs_module_file(worker_name);
	if (worker_file == nullptr)
		return false;

	const fs::path worker(worker_file);
	bfree(worker_file);

	if (!fs::is_regular_file(worker)) {
		obs_log(LOG_ERROR, "OBS RVC worker executable not found: %s", worker.string().c_str());
		return false;
	}

	*process = new rvc_worker_process{};
	if (!launch(**process, worker)) {
		delete *process;
		*process = nullptr;
		obs_log(LOG_ERROR, "Unable to start OBS RVC worker");
		return false;
	}

	obs_log(LOG_INFO, "OBS RVC worker started");
	return true;
}

void rvc_worker_stop(rvc_worker_process_t *process)
{
	if (process == nullptr)
		return;
#ifdef _WIN32
	if (process->process.hProcess != nullptr) {
		TerminateProcess(process->process.hProcess, 0U);
		WaitForSingleObject(process->process.hProcess, 5000U);
		CloseHandle(process->process.hThread);
		CloseHandle(process->process.hProcess);
	}
#else
	if (process->pid > 0) {
		kill(process->pid, SIGTERM);
		waitpid(process->pid, nullptr, 0);
	}
#endif
	delete process;
}
