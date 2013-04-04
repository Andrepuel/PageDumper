#include <fstream>
#include <string>
#include <cassert>
#include <iostream>
#include <sstream>

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

struct ToastPointer {
	uint8 pointer_size;
	varatt_external pointer;

	std::string build_string() {
		std::stringstream r;
		varatt_external pointer_here;
		memcpy(&pointer_here,reinterpret_cast<char*>(this)+1,sizeof(varatt_external));

		r << "TOAST_POINTER[" << int(pointer_size) << "] = {";
		r << pointer_here.va_rawsize << ",";
		r << pointer_here.va_extsize << ",";
		r << pointer_here.va_valueid << ",";
		r << pointer_here.va_toastrelid;
		r << "}";
		return r.str();
	}
};
struct Varlena : public varlena {
	unsigned _size() {
		return *reinterpret_cast<unsigned*>(vl_len_);
	}
	
	enum LenType {
		NORMAL,COMPRESSED,TOASTED,ONE_BYTE
	};

	LenType len_type() {
		if( vl_len_[0] & 0x1 ) {
			if( vl_len_[0] == 1 ) {
				return TOASTED;
			} else {
				return ONE_BYTE;
			}
		} else {
			if( (vl_len_[0] & 0x2) == 0 ) {
				return NORMAL;
			} else {
				return COMPRESSED;
			}
		}
	};

	ToastPointer* get_toast_pointer() {
		assert( len_type() == TOASTED );
		return reinterpret_cast<ToastPointer*>( reinterpret_cast<char*>(this)+1 );
	}

	unsigned size() {
		switch( len_type() ) {
		case NORMAL: return _size()>>2;
		case COMPRESSED: return _size()>>2;
		case TOASTED: return 18;
		case ONE_BYTE: return (_size()&0xFF)>>1;
		}
		assert(false);
		return 0;
	}

	std::string build_string() {
		unsigned size_str = size();

		char* data = reinterpret_cast<char*>(this);
		switch( len_type() ) {
		case COMPRESSED: return "COMPRESSED";
		case TOASTED: return get_toast_pointer()->build_string();
		case NORMAL: data += 4; size_str -= 4; break;
		case ONE_BYTE: data += 1; size_str -= 1; break;
		}

		std::string r;
		for( unsigned int i=0;i<size_str;++i ) {
			r += data[i];
		}
		return r;
	};
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
			if( data[i] >= '!' && data[i] <= '~') {
				result += data[i];
			} else if( data[i] == '\n' ) {
				result += "\\n";
			} else if( data[i] == ' ') {
				result += " ";
			} else {
//				result += std::to_string( data[i] );
				result += hexadecimate( ((unsigned char)(data[i]))>>4 );
				result += hexadecimate( data[i]&0xF );
//				result += "X";
			}
			result += " ";
		}
		return result;
	}
	
	//TODO Assuming numbers are word aligned, check this
	unsigned word_padding(unsigned pos) {
		return (4-(pos%4))%4;
	}
	
	unsigned get_unsigned(unsigned pos) {
		return *reinterpret_cast<unsigned*>( data+pos+word_padding(pos) );
	};
	unsigned get_unsigned_size(unsigned pos) {
		return 4+word_padding(pos);
	}

	int get_integer(unsigned pos) {
		return *reinterpret_cast<int*>( data+pos+word_padding(pos) );
	};
	int get_integer_size(unsigned pos) {
		return 4+word_padding(pos);
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
		if( !has_bitmap() )
			return true;
		return !bool(att_isnull(pos,t_bits));
	}
};

struct PageData : public PageHeaderData {
	unsigned itemid_count() {
		return integral_division( pd_lower-sizeof(PageHeaderData)+sizeof(ItemIdData), sizeof(ItemIdData) );
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
		std::cout << "Page " << which_page << "(" << page.itemid_count() << " itens)" << std::endl;
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

void print_integer(std::ostream& stream, const std::string& field, unsigned value, bool notNull) {
	stream << "\t\t" << field << " "; 
	if( notNull ) {
		stream << value;
	} else {
		stream << "NULL";
	}
	stream << std::endl;

}
void print_integer(std::ostream& stream, const std::string& field, int value, bool notNull) {
	stream << "\t\t" << field << " "; 
	if( notNull ) {
		stream << value;
	} else {
		stream << "NULL";
	}
	stream << std::endl;

}
void print_varlena(std::ostream& stream, const std::string& field, Varlena* varlena) {
	stream << "\t\t" << field << " ";
	if( varlena == NULL ) {
		stream << "NULL";
	} else {
		stream << "(" << varlena->size() << ") \"" << varlena->build_string() << "\"";
	}
	stream << std::endl;
}
void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata) {
	unsigned pos=0;
#if 0 //TOAST table code, TODO refactor
	assert( itemheader.has_attribute(0) );
	assert( itemheader.has_attribute(1) );
	assert( itemheader.has_attribute(2) );

	unsigned chunk_id = itemdata.get_unsigned(pos);
	pos += itemdata.get_unsigned_size(pos);

	unsigned chunk_seq = itemdata.get_unsigned(pos);
	pos += itemdata.get_unsigned_size(pos);

	Varlena* chunk_data = itemdata.get_varlena(pos);
	pos += itemdata.get_varlena_size(pos);

	std::cout << "\t\t{" << chunk_id << "," << chunk_seq << ",[" << chunk_data->size() << "]}" << std::endl;

	return;
#endif
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
	print_varlena(std::cout,"point",point);
	assert( pos == itemdata.data_size );
};
