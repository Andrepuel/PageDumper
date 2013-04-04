#include "page_dumper.h"
#include <stdexcept>
#include <fstream>

namespace page_dumper {

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

std::vector<char> bytes_from(const std::string& str) {
	return std::vector<char>(str.begin(),str.end());
};

ToastMap* Varlena::toastmap=NULL;

PageData* read_page(std::istream& stream, unsigned pos, char* buffer) {
	static char read_page_buffer[PAGE_SIZE];
	if( buffer == NULL ) buffer = read_page_buffer;

	stream.seekg(pos*PAGE_SIZE,std::istream::beg);
	stream.read(buffer,PAGE_SIZE);
	return reinterpret_cast<PageData*>(buffer);
}

void ToastMap::fill_toast_map() {
	static char page_buffer[PAGE_SIZE];

	PageIterator it(file);

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

				_ChunkData& data = positions[chunk_id];
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
void print_geometric(std::ostream& stream, const std::string& field, Varlena* varlena) {
	if( varlena == NULL ) {
		print_varlena(stream,field,varlena);
		return;
	}
	stream << "\t\t" << field << " ";
	std::vector<char> bytes = varlena->build_bytes();
	for( unsigned int i=0;i<bytes.size();++i ) {
		stream << hexadecimate( ((unsigned char)(bytes[i]))>>4 );
		stream << hexadecimate( ((unsigned char)(bytes[i]))&0xF );
	}
	stream << std::endl;
}

PageIterator::PageIterator(std::istream* stream)
: stream(stream), it(0)
{
	assert( stream->good() );

	stream->seekg(0, std::ifstream::end);
	unsigned size = stream->tellg();
	stream->seekg(0, std::ifstream::beg);
	end = size/PAGE_SIZE;

	std::stringstream err;
	if( size - (end*PAGE_SIZE) != 0 ) {
		err << "Given file is not aligned to " << PAGE_SIZE << " bytes" << std::endl;
		throw std::logic_error(err.str());
	}
}

PageData* PageIterator::next(char* buffer) {
	if( it >= end )
		return NULL;

	return read_page(*stream,it++,buffer);
}



};
