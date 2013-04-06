#include "postgis_support.h"
#include <cassert>

extern "C" {
#include <wktparse.h>
}

namespace page_dumper {

std::vector<char> postgis_to_WKB(const std::vector<char>& bytes) {
	size_t binary_size;
	char* c_result = unparse_WKB( const_cast<uchar*>(reinterpret_cast<const uchar*>(&bytes[0])), malloc,free,1, &binary_size,0);
	std::vector<char> result(c_result,c_result+binary_size);
	free(c_result);

	return result;
}

};
