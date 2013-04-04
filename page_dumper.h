#ifndef __PAGE_DUMPER_H_04172981780543128412041260481_
#define __PAGE_DUMPER_H_04172981780543128412041260481_

#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>

extern "C" {
#include <pg_config.h>
#include <postgres.h>
#include <storage/bufpage.h>
#include <storage/itemid.h>
#include <access/htup.h>
}

#define PAGE_SIZE (8*1024)
namespace page_dumper {

char hexadecimate(unsigned value);

inline unsigned integral_division(unsigned dividend, unsigned divisor) {
	unsigned result = dividend/divisor;
	assert( dividend - (divisor*result) == 0 );
	return result;
}

std::vector<char> bytes_from(const std::string& str);

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

	std::vector<char> build_bytes() {
		unsigned size_str = size();

		char* data = reinterpret_cast<char*>(this);
		switch( len_type() ) {
		case COMPRESSED: return bytes_from("COMPRESSED");
		case TOASTED: return bytes_from(get_toast_pointer()->build_string());
		case NORMAL: data += 4; size_str -= 4; break;
		case ONE_BYTE: data += 1; size_str -= 1; break;
		}

		std::vector<char> r;
		for( unsigned int i=0;i<size_str;++i ) {
			r.push_back(data[i]);
		}
		return r;
	}

	std::string build_string() {
		auto bytes = build_bytes();
		return std::string(bytes.begin(),bytes.end());
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

PageData* read_page(std::istream& stream, unsigned pos, char* buffer=NULL);

void print_integer(std::ostream& stream, const std::string& field, unsigned value, bool notNull);
void print_integer(std::ostream& stream, const std::string& field, int value, bool notNull);
void print_varlena(std::ostream& stream, const std::string& field, Varlena* varlena);
void print_geometric(std::ostream& stream, const std::string& field, Varlena* varlena);

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata, int argc, char** argv);

}

#endif
