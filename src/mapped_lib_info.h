#ifndef mapped_lib_info_h
#define mapped_lib_info_h

#include <string>
#include <set>
using namespace std;

class mapped_lib_info {

	string m_path;
	unsigned long m_start_addr;

	string m_filename;
	unsigned long m_length;
public:
	mapped_lib_info(const char *path);
};

typedef set<mapped_lib_info> mapped_lib_info_set_t;
typedef set<mapped_lib_info> mapped_lib_info_set_itr;

#endif
