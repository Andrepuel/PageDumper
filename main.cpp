#include <fstream>
#include <string>

#include "page_dumper.h"

using namespace page_dumper;

int main(int argc, char** argv) {
	assert( sizeof(PageData) == 24+sizeof(ItemIdData) );
	assert( sizeof(ItemIdData) == 4 );

	if( argc < 2 ) {
		std::cerr << "Usage " << argv[0] << " input_file" << std::endl;
		return 1;
	}

	std::ifstream input(argv[1]);
	assert( input.good() );

	input.seekg(0, std::ifstream::end);
	unsigned size = input.tellg();
	input.seekg(0, std::ifstream::beg);
	unsigned pages = size/PAGE_SIZE;
	if( size - (pages*PAGE_SIZE) != 0 ) {
		std::cerr << "Given file is not aligned to " << PAGE_SIZE << " bytes" << std::endl;
		return 1;
	}

	if( pages == 0 ) {
		std::cerr << "Given file has zero pages" << std::endl;
		return 1;
	};

	for( unsigned int which_page=0;which_page<pages;++which_page) {
		PageData& page = *read_page(input,which_page);
		std::cerr << "Page " << which_page << "(" << page.itemid_count() << " itens)" << std::endl;
		for( unsigned int i = 0; i < page.itemid_count(); ++i ) {
			ItemIdData& itemid = page.pd_linp[i];
			if( itemid.lp_flags != LP_NORMAL ) {
				std::cerr << "\tIgnoring item because it is not marked as LP_NORMAL" << std::endl;
			} else if( itemid.lp_len == 0 ) {
				std::cerr << "\tIgnoring item because it has no storage" << std::endl;
			} else if( itemid.lp_off > PAGE_SIZE ) {
				std::cerr << "\tIgnoring item because it's offset is outside of page" << std::endl;
			} else if( itemid.lp_off + itemid.lp_len > PAGE_SIZE ) {
				std::cerr << "\tIgnoring item because it spans outsize of the page" << std::endl;
			} else {
				ItemHeader& itemheader = *page.get_item_header(itemid);
				ItemData itemdata = page.get_item_data(itemid);
				user_code(itemid,itemheader,itemdata, argc, argv);
			}
		};
	}

	return 0;
};
