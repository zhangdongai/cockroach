#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <string>
using namespace std;

#include <errno.h>
#include <dlfcn.h>

#include "utils.h"
#include "mapped_lib_manager.h"
#include "probe_info.h"
#include "time_measure_probe.h"

static void *(*g_orig_dlopen)(const char *, int) = NULL;
static int (*g_orig_dlclose)(void *) = NULL;

typedef list<probe_info> probe_info_list_t;
typedef probe_info_list_t::iterator  probe_info_list_itr;

class cockroach {
	mapped_lib_manager m_mapped_lib_mgr;
	probe_info_list_t m_probe_list;

	static void _parse_one_recipe(const char *line, void *arg);
	void parse_recipe(const char *recipe_file);
	void parse_one_recipe(const char *line);
	void parse_time_measurement(vector<string> &tokens);
public:
	cockroach(void);
};

cockroach::cockroach(void)
{
	printf("started cockroach (%s)\n", __DATE__);
	g_orig_dlopen = (void *(*)(const char *, int))dlsym(RTLD_NEXT, "dlopen");
	if (!g_orig_dlopen) {
		printf("Failed to call dlsym() for dlopen.\n");
		exit(EXIT_FAILURE);
	}

	g_orig_dlclose = (int (*)(void *))dlsym(RTLD_NEXT, "dlclose");
	if (!g_orig_dlclose) {
		printf("Failed to call dlsym() for dlclose.\n");
		exit(EXIT_FAILURE);
	}

	// parse recipe file
	char *recipe_file = getenv("COCKROACH_RECIPE");
	if (!recipe_file) {
		printf("Not defined environment variable: COCKROACH_RECIPE\n");
		exit(EXIT_FAILURE);
	}
	parse_recipe(recipe_file);

	// install probes for libraries that have already been mapped.
	probe_info_list_itr probe = m_probe_list.begin();
	for (; probe != m_probe_list.end(); ++probe) {
		const char *target_name = probe->get_target_lib_path();
		const mapped_lib_info* lib_info;
		lib_info = m_mapped_lib_mgr.get_lib_info(target_name);
		if (!lib_info)
			continue;
		probe->install(lib_info);
	}
}

void cockroach::parse_time_measurement(vector<string> &tokens)
{
	if (tokens.size() != 4) {
		printf("Invalid format: tokens(%zd) != 3\n", tokens.size());
		for (int i = 0; i < tokens.size(); i++) {
			printf("[%d] %s\n", i, tokens[i].c_str());
		}
		exit(EXIT_FAILURE);
	}
}

void cockroach::parse_one_recipe(const char *line)
{
	// strip head spaces
	vector<string> tokens = utils::split(line);
	if (tokens.empty())
		return;

	if (tokens[0].at(0) == '#') // comment line
		return;

	if (tokens[0] == "T")
		parse_time_measurement(tokens);
	else {
		printf("Unknown command: '%s' : %s\n", tokens[0].c_str(), line);
		exit(EXIT_FAILURE);
	}

	// target address
	string &target_lib = tokens[1];
	string &sym_or_addr = tokens[2];
	unsigned long target_addr = 0;
	if (utils::is_hex_number(sym_or_addr.c_str())) {
		if (sscanf(sym_or_addr.c_str(), "%lx", &target_addr) != 1) {
			printf("Failed to parse address: %s: %s", 
			       target_lib.c_str(), sym_or_addr.c_str());
			exit(EXIT_FAILURE);
		}
	} else {
		printf("Currently symbol is not implemented. "
		       "Please specify address. %s: %s\n",
		       target_lib.c_str(), sym_or_addr.c_str());
		exit(EXIT_FAILURE);
	}

	int overwrite_length = atoi(tokens[3].c_str());

	// register probe
	probe_info probe(PROBE_TYPE_OVERWRITE_JUMP);
	probe.set_target_address(target_lib.c_str(), target_addr,
	                         overwrite_length);
	probe.set_probe(NULL, roach_time_measure_probe);
	probe.set_ret_probe(roach_time_mesure_ret_probe);

	m_probe_list.push_back(probe);
}

void cockroach::_parse_one_recipe(const char *line, void *arg)
{
	cockroach *obj = static_cast<cockroach *>(arg);
	obj->parse_one_recipe(line);
}

void cockroach::parse_recipe(const char *recipe_file)
{
	bool ret = utils::read_one_line_loop(recipe_file,
	                                     cockroach::_parse_one_recipe,
	                                     this);
	if (!ret) {
		printf("Failed to parse recipe file: %s (%d)\n", recipe_file,
		       errno);
		exit(EXIT_FAILURE);
	}
}

// make instance
static cockroach roach;

// --------------------------------------------------------------------------
// wrapper functions
// --------------------------------------------------------------------------
/**
 * dlopen() wrapper to call dlopen_hook_start() if necesary.
 *
 * @param filename of dynamic library (full path).
 * @param flag dlopen() flags.
 * @returns an opaque "handle" for the dynamic library.
 */
extern "C"
void *dlopen(const char *filename, int flag)
{
	void *handle = (*g_orig_dlopen)(filename, flag);
	/*
	map<string, workaround_runner *>::iterator it;
	it = g_dlopen_runner_map.find(filename);
	if (it != g_dlopen_runner_map.end()) {
		workaround_runner *runner = it->second;
		runner->dlopen_hook_start(handle, filename);
	}*/
	return handle;
}

/**
 * just calling glibc dlclose().
 */
extern "C"
int dlclose(void *handle) __THROW
{
	return (*g_orig_dlclose)(handle);
}

