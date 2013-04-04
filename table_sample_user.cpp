#include "page_dumper.h"

namespace page_dumper {

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata, int, char**) {
	unsigned pos=0;

	std::cout << "\tItem (" << itemid.lp_off << "," << itemid.lp_len << ")~" << itemid.lp_flags;
	std::cout << ", number of attributes is " << itemheader.number_attributes();
	std::cout << " " << (itemheader.has_bitmap() ? "with" : "without") << " bitmap"; 
	if( itemheader.has_bitmap() ) {
		std::cout << ":\t";
		for( unsigned int j=0; j < itemheader.number_attributes(); ++j) {
			std::cout << " " << itemheader.has_attribute(j);
		}
	}
	std::cout << ", data size is " << itemdata.data_size << std::endl;

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
}

};
