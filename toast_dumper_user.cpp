#include "page_dumper.h"
#include <sstream>

namespace {
unsigned filter = 0;
}

namespace page_dumper {

void user_code_finish() {
}

void user_code_setup(int argc, char** argv) {
	if( argc > 2 ) {
		std::stringstream read_number(argv[2]);
		read_number >> filter;
	}
}

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata) {
	unsigned pos=0;
	assert( itemheader.has_attribute(0) );
	assert( itemheader.has_attribute(1) );
	assert( itemheader.has_attribute(2) );

	unsigned chunk_id = itemdata.get_unsigned(pos);
	pos += itemdata.get_unsigned_size(pos);

	unsigned chunk_seq = itemdata.get_unsigned(pos);
	pos += itemdata.get_unsigned_size(pos);

	Varlena* chunk_data = itemdata.get_varlena(pos);
	std::string chunk_hex = itemdata.hexadecimate_all(pos,pos+30);
	pos += itemdata.get_varlena_size(pos);

	if( filter == 0 || chunk_id == filter ) {
		std::cout << "\t\t{" << chunk_id << "," << chunk_seq << ",[" << chunk_data->size() << "]\t{" << chunk_hex << "}}" << std::endl;
	}

	return;
}

}
