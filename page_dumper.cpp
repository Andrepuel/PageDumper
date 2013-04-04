#include "page_dumper.h"

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

PageData* read_page(std::istream& stream, unsigned pos, char* buffer) {
	static char read_page_buffer[PAGE_SIZE];
	if( buffer == NULL ) buffer = read_page_buffer;

	stream.seekg(pos*PAGE_SIZE,std::istream::beg);
	stream.read(read_page_buffer,PAGE_SIZE);
	return reinterpret_cast<PageData*>(&read_page_buffer);
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
	if( varlena == NULL || varlena->len_type() == Varlena::TOASTED ) {
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

};
