#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
using namespace std;

#include <errno.h>

#include "mapped_lib_manager.h"
#include "utils.h"

#define NUM_SHARED_LIB_MAP_ARGS      6
#define IDX_SHARED_LIB_MAP_ADDR_RAGE 0
#define IDX_SHARED_LIB_MAP_PERM      1
#define IDX_SHARED_LIB_MAP_PATH      5

#define LEN_SHARED_LIB_MAP_PERM      4
#define IDX_PERM_EXEC                2

// --------------------------------------------------------------------------
// private functions
// --------------------------------------------------------------------------
void mapped_lib_manager::parse_mapped_lib_line(const char *line)
{
	// strip head spaces
	vector<string> tokens = utils::split(line);
	if (tokens.size() != NUM_SHARED_LIB_MAP_ARGS)
		return;

	// select path that starts from '/'
	string &path = tokens[IDX_SHARED_LIB_MAP_PATH];
	if (path.at(0) != '/')
		return;

	// select permission that contains execution
	string &perm = tokens[IDX_SHARED_LIB_MAP_PERM];
	if (perm.size() < LEN_SHARED_LIB_MAP_PERM)
		return;
	if (perm[IDX_PERM_EXEC] != 'x')
		return;

	// parse start address
	string &addr_range = tokens[0];
	vector<string> addrs = utils::split(addr_range.c_str(), '-');
	if (addrs.size() != 2)
		return;
	unsigned long start_addr = 0;

	//m_shared_lib_map.insert(pair<string, unsigned long>(path, start_addr));
	printf("line: %s", line);
}

// --------------------------------------------------------------------------
// public functions
// --------------------------------------------------------------------------
mapped_lib_manager::mapped_lib_manager()
{
	const char *map_file_path = "/proc/self/maps";
	FILE *fp = fopen(map_file_path, "r");
	if (!fp) {
		printf("Failed to open: %s (%d)\n", map_file_path, errno);
		exit(EXIT_FAILURE);

	}
	static const int MAX_CHARS_ONE_LINE = 4096;
	char buf[MAX_CHARS_ONE_LINE];
	while (true) {
		char *ret = fgets(buf, MAX_CHARS_ONE_LINE, fp);
		if (!ret)
			break;
		parse_mapped_lib_line(buf);
	}
	fclose(fp);
}

