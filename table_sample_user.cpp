#include "page_dumper.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>

namespace {
using namespace page_dumper;

std::map< _ChunkId, _ChunkData > toast;
std::ifstream toast_file;

//TODO Put this code in the shared header
void toast_table_init(int argc, char** argv) {
	static bool inited=false;

	if( inited )
		return;
	inited=true;

	static char page_buffer[PAGE_SIZE];

	if( argc <= 2 ) {
		std::cerr << "Usage " << argv[0] << " input_table table's_toast" << std::endl;
		throw 1;
	}

	toast_file.open(argv[2]);
	PageIterator it(&toast_file);


	//TODO Move out of here
	PageData* each;
	unsigned int page_pos=0;
	while( (each = it.next(page_buffer)) != NULL ) {
		PageData& page = *each;

		for( unsigned int i = 0; i < page.itemid_count(); ++i ) {
			ItemIdData& itemid = page.pd_linp[i];

			if( itemid.lp_flags != LP_NORMAL ) {
			} else if( itemid.lp_len == 0 ) {
			} else if( itemid.lp_off > PAGE_SIZE ) {
			} else if( itemid.lp_off + itemid.lp_len > PAGE_SIZE ) {
			} else {
				ItemHeader& itemheader = *page.get_item_header(itemid);
				ItemData itemdata = page.get_item_data(itemid);

				unsigned pos=0;
				assert( itemheader.has_attribute(0) );
				assert( itemheader.has_attribute(1) );
				assert( itemheader.has_attribute(2) );

				unsigned chunk_id = itemdata.get_unsigned(pos);
				pos += itemdata.get_unsigned_size(pos);

				unsigned chunk_seq = itemdata.get_unsigned(pos);
				pos += itemdata.get_unsigned_size(pos);

				Varlena* chunk_data = itemdata.get_varlena(pos);
				pos += itemdata.get_varlena_size(pos);

				_ChunkData& data = toast[chunk_id];
				if( data.size() <= chunk_seq ) {
					data.resize( chunk_seq+1 );
				}

				//			PAGEOFFSET	     ITEMOFFSET      ITEMHEADEROFFSET	ID AND SEQ 	VARLENA OFFSET
				data[chunk_seq].first = PAGE_SIZE*page_pos + itemid.lp_off + itemheader.t_hoff + 8		 + chunk_data->data_offset();
				data[chunk_seq].second = chunk_data->size() - chunk_data->data_offset();
			}
		}
	
		page_pos++;
	};
}

}

namespace page_dumper {

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata, int argc, char** argv) {
	toast_table_init(argc,argv);
	Varlena::toast_positions = &toast;
	Varlena::toast_file = &toast_file;

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

	Varlena::toast_positions = NULL;
	Varlena::toast_file = NULL;
}

};
