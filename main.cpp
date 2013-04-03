#include <fstream>
#include <string>
#include <cassert>
#include <iostream>

#include <stdio.h>

extern "C" {
#include <pg_config.h>
#include <postgres.h>
#include <storage/bufpage.h>
#include <storage/itemid.h>
#include <access/htup.h>
}
#define PAGE_SIZE (8*1024)

namespace {

char hexadecimate(unsigned value) {
	switch( value ) {
	case 0: return '0';
	case 1: return '1';
	case 2: return '2';
	case 3: return '3';
	case 4: return '4';
	case 5: return '5';
	case 6: return '6';
	case 7: return '7';
	case 8: return '8';
	case 9: return '9';
	case 10: return 'A';
	case 11: return 'B';
	case 12: return 'C';
	case 13: return 'D';
	case 14: return 'E';
	case 15: return 'F';
	}
	assert(false);
	return '?';
}

inline unsigned integral_division(unsigned dividend, unsigned divisor) {
	unsigned result = dividend/divisor;
	assert( dividend - (divisor*result) == 0 );
	return result;
}

struct Varlena : public varlena {
	unsigned _size() {
		return *reinterpret_cast<unsigned*>(vl_len_);
	}
	unsigned size() {
		assert(false);//WIP
		return _size();
	}
};

struct ItemHeader;
struct ItemData {
	ItemHeader* header;
	char* data;
	unsigned data_size;

	std::string hexadecimate_all(unsigned pos=0,unsigned max=-1) {
		std::string result;
		unsigned end = data_size;
		if( end > max )
			end = max;

		for( unsigned int i = pos; i < end; ++i ) {
			if( isalnum(data[i]) ) {
				result += data[i];
			} else if( data[i] == '\n' ) {
				result += "\\n";
			} else if( data[i] == ' ') {
				result += " ";
			} else {
//				result += std::to_string( data[i] );
//				result += hexadecimate( ((unsigned char)(data[i]))>>4 );
//				result += hexadecimate( data[i]&0xF );
				result += "X";
			}
//			result += " ";
		}
		return result;
	}
	
	unsigned get_serial(unsigned pos) {
		return *reinterpret_cast<unsigned*>( data+pos );
	};
	unsigned get_serial_size() {
		return 4;
	}

	Varlena* get_varlena(unsigned pos) {
		return reinterpret_cast<Varlena*>( data+pos );
	}
	unsigned get_varlena_size(unsigned pos) {
		return get_varlena(pos)->size();
	}

};

struct ItemHeader : public HeapTupleHeaderData {
	unsigned number_attributes() {
		return unsigned(t_infomask2 & HEAP_NATTS_MASK);
	}

	bool has_bitmap() {
		return bool(t_infomask & HEAP_HASNULL);
	}

	bool has_attribute(unsigned pos) {
		return !bool(att_isnull(pos,t_bits));
	}
};

struct PageData {
	union {
		PageHeaderData header;
		ItemIdData itens[1];
	};

	unsigned itemid_count() {
		return integral_division( header.pd_lower-sizeof(PageHeaderData)+sizeof(ItemIdData), sizeof(ItemIdData) );
	};

	ItemHeader* get_item_header(const ItemIdData& itemid) {
		return reinterpret_cast<ItemHeader*>( reinterpret_cast<char*>(this)+itemid.lp_off );
	};

	ItemData get_item_data(const ItemIdData& itemid) {
		ItemHeader* header = get_item_header(itemid);
		char* data = reinterpret_cast<char*>(header) + header->t_hoff;
		unsigned data_size = itemid.lp_len - header->t_hoff;

		ItemData result = {header,data,data_size};
		return result;
	}
};

char read_page_buffer[PAGE_SIZE];
PageData* read_page(std::istream& stream, unsigned pos) {
	stream.seekg(pos*PAGE_SIZE,std::istream::beg);
	stream.read(read_page_buffer,PAGE_SIZE);
	return reinterpret_cast<PageData*>(&read_page_buffer);
}

}

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata);

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
//		std::cout << "Page " << which_page << std::endl;
		for( unsigned int i = 0; i < page.itemid_count(); ++i ) {
			ItemIdData& itemid = page.itens[i];
			if( itemid.lp_flags != LP_NORMAL ) {
//				std::cerr << "\tIgnoring item because it is not marked as LP_NORMAL" << std::endl;
			} else if( itemid.lp_len == 0 ) {
//				std::cerr << "\tIgnoring item because it has no storage" << std::endl;
			} else if( itemid.lp_off > PAGE_SIZE ) {
//				std::cerr << "\tIgnoring item because it's offset is outside of page" << std::endl;
			} else if( itemid.lp_off + itemid.lp_len > PAGE_SIZE ) {
//				std::cerr << "\tIgnoring item because it spans outsize of the page" << std::endl;
			} else {
				ItemHeader& itemheader = *page.get_item_header(itemid);
				std::cout << "\tItem (" << itemid.lp_off << "," << itemid.lp_len << ")~" << itemid.lp_flags;
				std::cout << ", number of attributes is " << itemheader.number_attributes();
				std::cout << " " << (itemheader.has_bitmap() ? "with" : "without") << " bitmap"; 
				if( itemheader.has_bitmap() ) {
					std::cout << ":\t";
					for( unsigned int j=0; j < itemheader.number_attributes(); ++j) {
						std::cout << " " << itemheader.has_attribute(j);
					}
				}
				ItemData itemdata = page.get_item_data(itemid);
				std::cout << ", data size is " << itemdata.data_size << std::endl;

				user_code(itemid,itemheader,itemdata);
			}
		};
	}

	return 0;
};

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata) {
	std::cout << itemdata.hexadecimate_all() << std::endl;

	return;
};
