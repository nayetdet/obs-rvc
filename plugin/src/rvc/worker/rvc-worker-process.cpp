#include "rvc-worker-process.h"

#include <obs-module.h>
#include <plugin-support.h>

#include <cerrno>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
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
	std::wstring command = L"\"" + path.wstring() + L"\"";
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	return CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
			      path.parent_path().c_str(), &startup, &worker.process) != FALSE;
}
#else
bool wait_for_exit(pid_t pid, std::chrono::milliseconds timeout)
{
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	for (;;) {
		const pid_t result = waitpid(pid, nullptr, WNOHANG);
		if (result == pid || (result < 0 && errno == ECHILD))
			return true;
		if (result < 0 && errno != EINTR)
			return false;
		if (std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

bool launch(rvc_worker_process_t &worker, const fs::path &path)
{
	worker.pid = fork();
	if (worker.pid < 0)
		return false;
	if (worker.pid == 0) {
#if defined(__linux__)
		if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0 || getppid() == 1)
			_exit(127);
#endif
		if (chdir(path.parent_path().c_str()) != 0)
			_exit(127);
		execl(path.c_str(), path.c_str(), nullptr);
		_exit(127);
	}
	return true;
}
#endif
}

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
		if (!wait_for_exit(process->pid, std::chrono::seconds(5))) {
			kill(process->pid, SIGKILL);
			wait_for_exit(process->pid, std::chrono::seconds(5));
		}
	}
#endif
	delete process;
}
