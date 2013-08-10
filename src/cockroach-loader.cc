#include <cstdio>
#include <cstdlib>
#include <string>
#include <set>
#include <sstream>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/types.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

using namespace std;

struct context {
	string cockroach_lib_path;
	string recipe_path;
	pid_t  target_pid;
	struct user_regs_struct regs;
	unsigned long install_trap_addr;
	unsigned long install_trap_orig_code;
	set<pid_t>    thread_ids;

	context(void)
	: target_pid(0),
	  install_trap_addr(0)
	{
	}
};

#define TRAP_INSTRUCTION 0xcc

#define PRINT_USAGE_AND_EXIT_IF_THE_LAST(i, argc) \
do { \
	if (i == argc - 1) { \
		print_usage(); \
		return EXIT_FAILURE; \
	} \
} while(0)

#define INC_INDEX_AND_UPDATE_ARG(i, argv, arg) \
do { \
	i++; \
	arg = argv[i]; \
} while(0)

/*
static int tkill(int tid, int signo)
{
	return syscall(SYS_tkill, tid, signo);
}*/

static string get_default_cockroach_lib_path(void)
{
	return "cockroach.so";
}

static string get_child_code_string(int code)
{
	string str;
	switch(code) {
	case CLD_EXITED:
		str = "CLD_EXITED";
		break;
	case CLD_KILLED:
		str = "CLD_KILLED";
		break;
	case CLD_DUMPED:
		str = "CLD_DUMPED";
		break;
	case CLD_STOPPED:
		str = "CLD_STOPPED";
		break;
	case CLD_TRAPPED:
		str = "CLD_TRAPPED";
		break;
	case CLD_CONTINUED:
		str = "CLD_CONTINUED";
		break;
	default:
		stringstream ss;
		ss << "Unknown child code: " << code;
		str = ss.str();
		break;
	}
	return str;
}

static bool wait_signal(pid_t pid, int *status = NULL,
                        int expected_code = CLD_TRAPPED)
{
	siginfo_t info;
	int result = waitid(P_PID, pid, &info, WEXITED|WSTOPPED);
	if (result == -1) {
		printf("Failed to wait for the stop of the target: %d: %s\n",
		       pid, strerror(errno));
		return false;
	}
	if (info.si_code != expected_code) {
		printf("waitid() returned with the unexpected code: %s "
		       "(expected: %s)\n",
		       get_child_code_string(info.si_code).c_str(),
		       get_child_code_string(expected_code).c_str());
		return false;
	}
	if (status)
		*status = info.si_status;

	return true;
}

static bool attach(pid_t tid)
{
	if (ptrace(PTRACE_ATTACH, tid, NULL, NULL)) {
		printf("Failed to attach the process: %d: %s\n",
		       tid, strerror(errno));
		return false;
	}
	printf("Attached: %d\n", tid);
	return true;
}

static bool attach_all_threads(context *ctx)
{
	int signo = SIGSTOP;
	if (ptrace(PTRACE_CONT, ctx->target_pid, NULL, signo) == -1) {
		printf("Failed: PTRACE_CONT. target: %d: %s. signo: %d\n",
		       ctx->target_pid, strerror(errno), signo);
		return false;
	}

	// wait for the stop of the child
	int status;
	if (!wait_signal(ctx->target_pid, &status))
		return false;
	if (status != SIGSTOP) {
		printf("wait() returned with unexpected status: %d\n", status);
		return false;
	}
	printf("Child process stopped.\n");

	// get all pids of threads
	stringstream proc_task_path;
	proc_task_path << "/proc/";
	proc_task_path << ctx->target_pid;
	proc_task_path << "/task";
	DIR *dir = opendir(proc_task_path.str().c_str());
	if (!dir) {
		printf("Failed: open dir: %s: %s\n",
		       proc_task_path.str().c_str(), strerror(errno));
		return false;
	}

	errno = 0;
	while (true) {
		struct dirent *entry = readdir(dir);
		if (!entry) {
			if (!errno)
				break;
			printf("Failed: read dir entry: %s: %s\n",
			       proc_task_path.str().c_str(), strerror(errno));
			if (closedir(dir) == -1) {
				printf("Failed: close dir: %s: %s\n",
				       proc_task_path.str().c_str(),
				       strerror(errno));
			}
			return false;
		}
		string dir_name(entry->d_name);
		if (dir_name == "." || dir_name == "..")
			continue;
		int tid = atoi(entry->d_name);
		printf("Found thread: %d\n", tid);
		ctx->thread_ids.insert(tid);
	}
	if (closedir(dir) == -1) {
		printf("Failed: close dir: %s: %s\n",
		       proc_task_path.str().c_str(), strerror(errno));
		return false;
	}

	// attach all threads
	set<pid_t>::iterator tid = ctx->thread_ids.begin();
	for (; tid != ctx->thread_ids.end(); ++tid) {
		// The main thread has already been attached
		if (*tid == ctx->target_pid)
			continue;
		if (!attach(*tid))
			return false;
	}

	return true;
}

static bool get_register_info(context *ctx)
{
	if (ptrace(PTRACE_GETREGS, ctx->target_pid, NULL, &ctx->regs)) {
		printf("Failed to get register info. target: %d: %s\n",
		       ctx->target_pid, strerror(errno));
		return false;
	}
	printf("RSP: %p, RIP: %p\n",
	       (void *)ctx->regs.rsp, (void *)ctx->regs.rip);
	return true;
}

/*
static bool send_signal_and_wait(context *ctx)
{
	int signo = SIGUSR2;
	printf("Send signal: %d (tid: %d)\n", signo, ctx->target_pid);
	if (tkill(ctx->target_pid, signo) == -1) {
		printf("Failed to send signal. target: %d: %s. signo: %d\n",
		       ctx->target_pid, strerror(errno), signo);
		return false;
	}
	if (ptrace(PTRACE_CONT, ctx->target_pid, NULL, NULL) == -1) {
		printf("Failed: PTRACE_CONT. target: %d: %s. signo: %d\n",
		       ctx->target_pid, strerror(errno), signo);
		return false;
	}
	int status;
	if (!wait_signal(ctx, &status))
		return false;
	printf("  => stopped. (%d)\n", status);
	return true;
}*/

static bool install_trap(context *ctx)
{
	void *addr = (void *)ctx->install_trap_addr;
	errno = 0;
	long orig_code = ptrace(PTRACE_PEEKTEXT, ctx->target_pid, addr, NULL);
	if (errno && orig_code == -1) {
		printf("Failed to PEEK_TEXT. target: %d: %s\n",
		       ctx->target_pid, strerror(errno));
		return false;
	}
	printf("code @ install trap point: %lx @ %p\n", orig_code, addr);

	// install trap
	long trap_code = orig_code;
	*((uint8_t *)&trap_code) = TRAP_INSTRUCTION;
	if (ptrace(PTRACE_POKETEXT, ctx->target_pid, addr, trap_code)) {
		printf("Failed to POKETEXT. target: %d: %s\n",
		       ctx->target_pid, strerror(errno));
		return false;
	}

	ctx->install_trap_orig_code = orig_code;
	return true;
}

static bool wait_install_trap(context *ctx)
{
	int status;
	if (!wait_signal(ctx->target_pid, &status))
		return false;
	printf("status: %d\n", status);
	return true;
}

static void print_usage(void)
{
	printf("Usage\n");
	printf("\n");
	printf("$ cockroach-loader [options] --recipe recipe_path pid\n");
	printf("\n");
	printf("Options\n");
	printf("  --install-trap-addr: The hex. address for the installation trap.\n");
	printf("  --cockroach-lib-path: The path of cockroach.so\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	context ctx;
	for (int i = 1; i < argc; i++) {
		string arg = argv[i];
		if (arg == "--cockroach-lib-path") {
			PRINT_USAGE_AND_EXIT_IF_THE_LAST(i, argc);
			INC_INDEX_AND_UPDATE_ARG(i, argv, arg);
			ctx.cockroach_lib_path = arg;

		} else if (arg == "--install-trap-addr") {
			PRINT_USAGE_AND_EXIT_IF_THE_LAST(i, argc);
			INC_INDEX_AND_UPDATE_ARG(i, argv, arg);
			sscanf(arg.c_str(), "%lx", &ctx.install_trap_addr);
		} else if (arg == "--recipe") {
			PRINT_USAGE_AND_EXIT_IF_THE_LAST(i, argc);
			ctx.recipe_path = arg;
		} else
			ctx.target_pid = atoi(arg.c_str());
	}

	if (ctx.cockroach_lib_path.empty())
		ctx.cockroach_lib_path = get_default_cockroach_lib_path();
	if (ctx.target_pid == 0) {
		printf("ERROR: You have to specify target PID.\n\n");
		print_usage();
		return EXIT_FAILURE;
	}

	printf("cockroach loader (build:  %s %s)\n", __DATE__, __TIME__);
	printf("lib path    : %s\n", ctx.cockroach_lib_path.c_str());
	printf("recipe path : %s\n", ctx.recipe_path.c_str());
	printf("target pid  : %d\n", ctx.target_pid);
	printf("install trap: %lx\n", ctx.install_trap_addr);

	// attach only the main thread to recieve SIGCHLD
	if (!attach(ctx.target_pid))
		return EXIT_FAILURE;
	// wait for the stop of the child
	if (!wait_signal(ctx.target_pid))
		return false;

	if (!attach_all_threads(&ctx))
		return EXIT_FAILURE;
	//if (!get_register_info(&ctx))
	//	return EXIT_FAILURE;
	//if (!send_signal_and_wait(&ctx))
	//	return EXIT_FAILURE;
	if (!install_trap(&ctx))
		return EXIT_FAILURE;
	if (!wait_install_trap(&ctx))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
