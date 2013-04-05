#include "page_dumper.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>

#ifdef WITH_POSGIS
#include "postgis_support.h"
#endif

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

void sql_varlena(std::ostream& stream, Varlena* varlena) {
	if( varlena == NULL ) {
		stream << "NULL";
		return;
	}
	std::string str = varlena->build_string();
	stream << "E'";
	for( auto each = str.begin(); each != str.end(); ++each ) {
		if( *each == '\\' ) {
			stream << "\\\\";
		} else if( *each == '\'' ) {
			stream << "''";
		} else {
			stream << *each;
		}
	}
	stream << '\'';
}

void sql_integer(std::ostream& stream, int number, bool notNull) {
	if( !notNull ) {
		stream << "NULL";
		return;
	}

	stream << number;
}

void sql_geometric(std::ostream& stream, Varlena* varlena);

void user_code_setup(int argc, char** argv) {
	toast_table_init(argc,argv);

	std::cout << "BEGIN;" << std::endl;
}

void user_code(ItemIdData& itemid, ItemHeader& itemheader, ItemData& itemdata) {
	static const std::string sql_insert("INSERT INTO sistema.grupo (id,name,text1,text2,text3,number1,text4,text5,number2,text6,point) VALUES ");
	static unsigned int insertion_count=0;

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
		std::stringstream all_output;

		if( insertion_count % 500 == 0 ) {
			if( insertion_count != 0 ) {
				all_output << ";" << std::endl;
			}
			all_output << sql_insert << std::endl;
		} else {
			all_output << "," << std::endl;
		}

		all_output << "\t(";

		sql_integer(all_output,id,true);
		all_output << ", ";
		sql_varlena(all_output,name);
		all_output << ", ";
		sql_varlena(all_output,text1);
		all_output << ", ";
		sql_varlena(all_output,text2);
		all_output << ", ";
		sql_varlena(all_output,text3);
		all_output << ", ";
		sql_integer(all_output,number1,itemheader.has_attribute(5));
		all_output << ", ";
		sql_varlena(all_output,text4);
		all_output << ", ";
		sql_varlena(all_output,text5);
		all_output << ", ";
		sql_integer(all_output,number2,itemheader.has_attribute(8));
		all_output << ", ";
		sql_varlena(all_output,text6);
		all_output << ", ";
		sql_geometric(all_output,point);
		all_output << ")";

		assert( pos == itemdata.data_size );

		std::cout << all_output.str();
		insertion_count++;
	} catch( const std::exception& e) {
		std::cerr << "\tException on item (" << itemid.lp_off << "," << itemid.lp_len << ")~" << itemid.lp_flags;
		std::cerr << ", number of attributes is " << itemheader.number_attributes();
		std::cerr << " " << (itemheader.has_bitmap() ? "with" : "without") << " bitmap"; 
		if( itemheader.has_bitmap() ) {
			std::cerr << ":\t";
			for( unsigned int j=0; j < itemheader.number_attributes(); ++j) {
				std::cerr << " " << itemheader.has_attribute(j);
			}
		}
		std::cerr << ", data size is " << itemdata.data_size;
		std::cerr << ". What( " << e.what() << ")" << std::endl;
	};

	Varlena::toastmap = NULL;
}

void user_code_finish() {
	std::cout << ";" << std::endl;
	std::cout << "COMMIT;" << std::endl;
}

void sql_geometric(std::ostream& stream, Varlena* varlena) {
	if( varlena == NULL ) {
		stream << "NULL";
	}
#ifdef WITH_POSTGIS
	std::vector<char> bytes = varlena->build_bytes();
	std::vector<char> binary_wkb = postgis_to_WKB( bytes );

	stream << '\'';
	for( unsigned int i=0;i<binary_wkb.size();++i ) {
		stream << hexadecimate( ((unsigned char)(binary_wkb[i]))>>4 );
		stream << hexadecimate( ((unsigned char)(binary_wkb[i]))&0xF );
	}
	stream << '\'';
#else
	std::cerr << "Warning: Postgis data not supported" << std::endl;
	stream << "NULL";
#endif
}

};
