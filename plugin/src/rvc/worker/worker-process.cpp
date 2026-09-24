#include "worker-process.h"

#include <obs-module.h>
#include <plugin-support.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <dlfcn.h>
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
bool launch(rvc_worker_process_t &worker, const std::vector<std::string> &args, const fs::path &directory)
{
	std::string command;
	for (const std::string &arg : args)
		command += (command.empty() ? "" : " ") + std::string("\"") + arg + "\"";

	STARTUPINFOA startup{};
	startup.cb = sizeof(startup);
	return CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
			      directory.string().c_str(), &startup, &worker.process) != FALSE;
}
#else
bool launch(rvc_worker_process_t &worker, const std::vector<std::string> &args, const fs::path &directory)
{
	worker.pid = fork();
	if (worker.pid < 0)
		return false;
	if (worker.pid == 0) {
		if (chdir(directory.c_str()) != 0)
			_exit(127);
		std::vector<char *> argv;
		for (const std::string &arg : args)
			argv.push_back(const_cast<char *>(arg.c_str()));
		argv.push_back(nullptr);
		execvp(argv[0], argv.data());
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

	fs::path directory;
	if (const char *configured = std::getenv("RVC_WORKER_DIR"); configured != nullptr) {
		directory = configured;
	} else {
#ifdef _WIN32
		HMODULE module = nullptr;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
					   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				   reinterpret_cast<LPCSTR>(&rvc_worker_start), &module);
		char filename[MAX_PATH]{};
		const DWORD length = GetModuleFileNameA(module, filename, MAX_PATH);
		directory = fs::path(std::string(filename, length)).parent_path() / ".." / ".." / "data" / "worker";
#else
		Dl_info info{};
		directory = dladdr(reinterpret_cast<void *>(&rvc_worker_start), &info) != 0
				    ? fs::path(info.dli_fname).parent_path() / ".." / ".." / "share" / "obs" /
					      "obs-plugins" / "obs-rvc" / "worker"
				    : fs::current_path() / "plugin-worker";
#endif
	}

	if (!fs::exists(directory)) {
		obs_log(LOG_ERROR, "OBS RVC worker directory not found: %s", directory.string().c_str());
		return false;
	}

	*process = new rvc_worker_process{};
	const char *executable = std::getenv("RVC_WORKER_EXECUTABLE");
	const char *python = std::getenv("RVC_WORKER_PYTHON");
	const std::vector<std::string> args =
		executable != nullptr
			? std::vector<std::string>{executable}
			: std::vector<std::string>{
				  python != nullptr ? python : "python3", "-c",
				  "import sys; sys.path.insert(0, 'src'); from plugin_worker.main import main; main()"};

	if (!launch(**process, args, directory)) {
		delete *process;
		*process = nullptr;
		obs_log(LOG_ERROR, "Unable to start OBS RVC worker");
		return false;
	}

	obs_log(LOG_INFO, "OBS RVC Python worker started");
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
