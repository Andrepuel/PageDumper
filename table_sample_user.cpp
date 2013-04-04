#include "page_dumper.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>

namespace {
using namespace page_dumper;

std::ifstream toast_file;
ToastMap toastmap = { std::map<_ChunkId,_ChunkData>() ,&toast_file};

void toast_table_init(int argc, char** argv) {
	static bool inited=false;

	if( inited )
		return;
	inited=true;

	if( argc <= 2 ) {
		std::cerr << "Usage " << argv[0] << " input_table table's_toast" << std::endl;
		throw 1;
	}

	toast_file.open(argv[2]);
	toastmap.fill_toast_map();
}

}

namespace page_dumper {

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata, int argc, char** argv) {
	toast_table_init(argc,argv);
	Varlena::toastmap = &toastmap;

	unsigned pos=0;

	assert( itemheader.has_attribute(0) );
	unsigned id = itemdata.get_unsigned(pos);
	pos += itemdata.get_unsigned_size(pos);

	assert( itemheader.has_attribute(1) );
	Varlena* name = itemdata.get_varlena(pos);
	pos += itemdata.get_varlena_size(pos);

	Varlena* text1 = NULL;
	if( itemheader.has_attribute(2) ) {
		text1 = itemdata.get_varlena(pos);
		pos += itemdata.get_varlena_size(pos);
	}

	Varlena* text2 = NULL;
	if( itemheader.has_attribute(3) ) {
		text2 = itemdata.get_varlena(pos);
		pos += itemdata.get_varlena_size(pos);
	}

	Varlena* text3 = NULL;
	if( itemheader.has_attribute(4) ) {
		text3 = itemdata.get_varlena(pos);
		pos += itemdata.get_varlena_size(pos);
	}

	int number1;
	if( itemheader.has_attribute(5) ) {
		number1 = itemdata.get_integer(pos);
		pos += itemdata.get_integer_size(pos);
	}

	Varlena* text4 = NULL;
	if( itemheader.has_attribute(6) ) {
		text4 = itemdata.get_varlena(pos);
		pos += itemdata.get_varlena_size(pos);
	}

	Varlena* text5 = NULL;
	if( itemheader.has_attribute(7) ) {
		text5 = itemdata.get_varlena(pos);
		pos += itemdata.get_varlena_size(pos);
	}

	int number2;
	if( itemheader.has_attribute(8) ) {
		number2 = itemdata.get_integer(pos);
		pos += itemdata.get_integer_size(pos);
	}

	Varlena* text6 = NULL;
	if( itemheader.has_attribute(9) ) {
		text6 = itemdata.get_varlena(pos);
		pos += itemdata.get_varlena_size(pos);
	}

	Varlena* point = NULL;
	if( itemheader.has_attribute(10) ) {
		point = itemdata.get_varlena(pos);
		pos += itemdata.get_varlena_size(pos);
	}

	try {
		print_integer(std::cout,"id",id,true);
		print_varlena(std::cout,"name",name);
		print_varlena(std::cout,"text1",text1);
		print_varlena(std::cout,"text2",text2);
		print_varlena(std::cout,"text3",text3);
		print_integer(std::cout,"number1",number1,itemheader.has_attribute(5));
		print_varlena(std::cout,"text4",text4);
		print_varlena(std::cout,"text5",text5);
		print_integer(std::cout,"number2",number2,itemheader.has_attribute(8));
		print_varlena(std::cout,"text6",text6);
		print_geometric(std::cout,"point",point);
		assert( pos == itemdata.data_size );

		std::cout << all_output.str() << std::endl;
	} catch( const std::exception& e) {
		std::cerr << "\tException on item (" << itemid.lp_off << "," << itemid.lp_len << ")~" << itemid.lp_flags;
		std::cerr << ", number of attributes is " << itemheader.number_attributes();
		std::cerr << " " << (itemheader.has_bitmap() ? "with" : "without") << " bitmap"; 
		if( itemheader.has_bitmap() ) {
			std::cout << ":\t";
			for( unsigned int j=0; j < itemheader.number_attributes(); ++j) {
				std::cout << " " << itemheader.has_attribute(j);
			}
		}
		std::cerr << ", data size is " << itemdata.data_size;
		std::cerr << ". What( " << e.what() << ")" << std::endl;
	};

	Varlena::toastmap = NULL;
}

};
