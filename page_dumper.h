#ifndef __PAGE_DUMPER_H_04172981780543128412041260481_
#define __PAGE_DUMPER_H_04172981780543128412041260481_

#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>
#include <map>
#include <stdexcept>

extern "C" {
#include <pg_config.h>
#include <postgres.h>
#include <storage/bufpage.h>
#include <storage/itemid.h>
#include <access/htup.h>
#include <utils/pg_lzcompress.h>
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

typedef unsigned _ChunkId;
typedef std::pair<size_t,size_t> _ChunkPos;
typedef std::vector<_ChunkPos> _ChunkData;

struct ToastMap {
	std::map<_ChunkId,_ChunkData> positions;
	std::istream* file;

	void fill_toast_map();
};

struct ToastPointer {
	uint8 pointer_size;

	varatt_external pointer() {
		varatt_external pointer_here;
		memcpy(&pointer_here,reinterpret_cast<char*>(this)+1,sizeof(varatt_external));
		return pointer_here;		
	}

	std::string build_string() {
		std::stringstream r;

		varatt_external pointer_here = pointer();

		r << "TOAST_POINTER[" << int(pointer_size) << "] = {";
		r << pointer_here.va_rawsize << ",";
		r << pointer_here.va_extsize << ",";
		r << pointer_here.va_valueid << ",";
		r << pointer_here.va_toastrelid;
		r << "}";
		return r.str();
	}

	bool is_compressed() {
		/* From postgres.h:
		 * "The data is compressed if and only if va_extsize < va_rawsize - VARHDRSZ"
		 */

		varatt_external pointer_here = pointer();
		return pointer_here.va_extsize < (pointer_here.va_rawsize-VARHDRSZ);
	}

	std::vector<char> fetch_data( ToastMap& toast ) {
		varatt_external pointer_here = pointer();

		auto found = toast.positions.find(pointer_here.va_valueid);
		if( found == toast.positions.end() ) {
			throw std::runtime_error("TOAST not found");
		}
		_ChunkData& datas = found->second;

		//Varlen must carry the compressed size
		unsigned size=4;
		for( unsigned int i=0;i<datas.size();++i) {
			size += datas[i].second;
		}

		std::vector<char> result(size);

		unsigned pos = 4;
		for( unsigned int i=0;i<datas.size();++i) {
			assert( pos+datas[i].second <= size );
			toast.file->seekg(datas[i].first,std::istream::beg);
			toast.file->read( &(result[pos]), datas[i].second );
			pos += datas[i].second;
		}
		assert( pos == size );

		if( !is_compressed() ) {
			//Toss varlen alway
			for( unsigned int i=0;i<result.size()-4;++i) {
				result[i+0] = result[i+0+4];
				result[i+1] = result[i+1+4];
				result[i+2] = result[i+2+4];
				result[i+3] = result[i+3+4];
			}
			result.resize(result.size()-4);
			return result;
		}

		PGLZ_Header& pglz_header = *reinterpret_cast<PGLZ_Header*>( &result[0] );
		pglz_header.vl_len_ = pointer_here.va_rawsize<<2;

		//WORKAROUND Extra kbyte to guarantee that a buggy decompresser does not write garbage where it shouldnt
		std::vector<char> decompressed(pglz_header.rawsize + 1024);
		pglz_decompress(&pglz_header, &(decompressed[0]) );
		decompressed.resize(pglz_header.rawsize);

		if( decompressed[0] == 'c' && decompressed[1] == 'c' ) {
			throw std::runtime_error("Corrupted compressed data");
		}

		return decompressed;
	}
};

struct Varlena : public varlena {
	static ToastMap* toastmap;

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

	unsigned data_offset() {
		switch( len_type() ) {
		case NORMAL: return 4;
		case ONE_BYTE: return 1;	
		default: return 0;
		}
		return 0;
	}

	std::vector<char> build_bytes() {
		unsigned size_str = size();

		char* data = reinterpret_cast<char*>(this);
		switch( len_type() ) {
		case COMPRESSED: return bytes_from("COMPRESSED"); //TODO
		case TOASTED: 
			if( toastmap != NULL ) {
				return get_toast_pointer()->fetch_data(*toastmap);
			} else {
				return bytes_from(get_toast_pointer()->build_string());
			}
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

#if 0 //PRINT ASCII
			if( data[i] >= '!' && data[i] <= '~') {
				result += data[i];
			} else if( data[i] == '\n' ) {
				result += "\\n";
			} else if( data[i] == ' ') {
				result += " ";
			} else 
#endif
			{
				result += hexadecimate( ((unsigned char)(data[i]))>>4 );
				result += hexadecimate( data[i]&0xF );
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

void user_code_setup(int argc, char** argv);
void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata);
void user_code_finish();

class PageIterator {
public:
	PageIterator(std::istream* stream);

	PageData* next(char* buffer);
private:
	std::istream* stream;
	unsigned it;
	unsigned end;
};

}

#endif
