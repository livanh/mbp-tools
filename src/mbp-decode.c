/*
	METADATA_BLOCK_PICTURE decoder: extracts information and binary data from a METADATA_BLOCK_PICTURE
	structure, as defined in https://xiph.org/flac/format.html#metadata_block_picture

	Copyright 2016 Livanh (livanh@bulletmail.org)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <endian.h>

FILE *infile;
FILE *outfile;

uint32_t read_32be_int();

int main( int argc, char** argv ) {
	
	char *infile_name = NULL;
	char *outfile_name = NULL;
	size_t bytes_read;
	
	 /* information produced as output
		0 = raw picture data
		1 = numeric picture type
		2 = descriptive picture type
		3 = picture MIME type
		4 = picture description
	*/
	int mode = -1;
	
	const char* mbp_type_description[] = {
		"Other",
		"32x32 pixel PNG file icon",
		"Other file icon",
		"Cover (front)",
		"Cover (back)",
		"Leaflet page",
		"Media (e.g. label side of CD)",
		"Lead artist/lead performer/soloist",
		"Artist/performer",
		"Conductor",
		"Band/Orchestra",
		"Composer",
		"Lyricist/text writer",
		"Recording Location",
		"During recording",
		"During performance",
		"Movie/video screen capture",
		"A bright coloured fish", // WTF??
		"Illustration",
		"Band/artist logotype",
		"Publisher/Studio logotype"
	};
	
	int      mbp_type = -1;
	uint32_t mbp_mime_length = 0;
	char*    mbp_mime_text = NULL;
	uint32_t mbp_description_length = 0;
	char*    mbp_description_text = NULL;
	uint32_t mbp_width = 0;
	uint32_t mbp_height = 0;
	uint32_t mbp_colordepth = 0;
	uint32_t mbp_palettesize = 0;
	uint32_t mbp_data_length = 0;
	char*    mbp_data = NULL;
	
	int c;
	opterr = 0;
	int help = 0;
	
	// process options
	while( ( c = getopt ( argc, argv, "pntmdho:" ) ) != -1 )
		switch( c ) {
			case 'p': mode = 0; break;
			case 'n': mode = 1; break;
			case 't': mode = 2; break;
			case 'm': mode = 3; break;
			case 'd': mode = 4; break;
			case 'h': help = 1; break;
			case 'o':
				outfile_name = optarg;
				break;
			case '?':
				if ( optopt == 'o' )
					fprintf ( stderr, "Error: option -%c requires an argument.\n", optopt);
				else if ( isprint( optopt ) )
					fprintf ( stderr, "Error: unknown option `-%c'.\n", optopt);
				else
					fprintf ( stderr, "Error: unknown option character `\\x%x'.\n", optopt);
				abort();
			default:
				abort();
		}
	
	// choose operating mode
	if( mode == -1 ) {
		fprintf( stderr, "METADATA_BLOCK_PICTURE decoder\n" );
		fprintf( stderr, "Extracts information and binary data from a METADATA_BLOCK_PICTURE structure\n" );
		fprintf( stderr, "Copyright 2016 Livanh (livanh@bulletmail.org)\n" );
		fprintf( stderr, "This program is released under the GNU GPL v3 (http://www.gnu.org/licenses/)\n" );
		fprintf( stderr, "\n" );
		if(!help) fprintf( stderr, "Error: no mode specified!\n" );
		if(!help) fprintf( stderr, "\n" );
		fprintf( stderr, "Usage: %s [<options>] [<input file>]\n", argv[0] );
		fprintf( stderr, "\n" );
		fprintf( stderr, "<input file> defaults to stdin\n" );
		fprintf( stderr, "\n" );
		fprintf( stderr, "Available options:\n" );
		fprintf( stderr, " -o <output file>     choose output file (if missing, stdout is used)\n" );
		fprintf( stderr, " -p                   extract binary picture data\n" );
		fprintf( stderr, " -n                   print numeric picture type\n" );
		fprintf( stderr, " -t                   print descriptive picture type\n" );
		fprintf( stderr, " -m                   print picture MIME type\n" );
		fprintf( stderr, " -d                   print picture description\n" );
		fprintf( stderr, " -h                   print this help\n" );
		fprintf( stderr, "\n" );
		fprintf( stderr, "One option between -p, -n, -t, -m or -d is mandatory\n" );
		fprintf( stderr, "If more than one is used, the last one wins\n" );
		fprintf( stderr, "\n" );
		return 1;
	} else {
		switch( mode ){
			case 0:  fprintf( stderr, "Mode 0: extract raw picture data\n" ); break;
			case 1:  fprintf( stderr, "Mode 1: print numeric picture type\n" ); break;
			case 2:  fprintf( stderr, "Mode 2: print descriptive picture type\n" ); break;
			case 3:  fprintf( stderr, "Mode 3: print picture MIME type\n" ); break;
			case 4:  fprintf( stderr, "Mode 4: print picture description\n" ); break;
			default: fprintf( stderr, "Error: invalid mode.\n" ); abort();
		}
	}
	
	// choose input
	if( optind == argc ) { // no arguments left: input is stdin
		fprintf( stderr, "Reading data from stdin\n" );
		infile = stdin;
	} else if( optind == argc-1 ) { // 1 argument left: it's the input filename
		infile_name = argv[ optind ];
		fprintf( stderr, "Reading data from file %s\n", infile_name );
		infile = fopen( infile_name, "rb" );
		if( infile == NULL ) { 
			fprintf( stderr, "Error: cannot open input file.\n" );
			abort();
		}
	} else { // 2 or more arguments left: it's an error
		fprintf( stderr, "Error: too many arguments.\n" );
		abort();
	}
	
	// choose output
	if( outfile_name == NULL ) {
		fprintf( stderr, "Writing data to stdout\n" );
		outfile = stdout;
	} else {
		fprintf( stderr, "Writing data to file %s\n", outfile_name );
		outfile = fopen( outfile_name, "wb" );
		if( outfile == NULL ) {
			fprintf( stderr, "Error: cannot open output file.\n" );
			abort();
		}
	}
	
	// --- read and process input data ---
	
	// picture type
	mbp_type = read_32be_int();
	if( mbp_type >= 0 && mbp_type <= 20 ) {
		fprintf( stderr, "Picture type: %d (%s)\n", mbp_type, mbp_type_description[ mbp_type ] );
	} else {
		fprintf( stderr, "Error: invalid picture type index, input data may be invalid.\n" );
		abort();
	}
	
	
	// picture MIME type
	mbp_mime_length = read_32be_int();
	mbp_mime_text = malloc( mbp_mime_length + 1 );
	if( mbp_mime_text == NULL ){
		fprintf( stderr, "Error: memory allocation failed.\n" );
		abort();
	}
	bytes_read = fread( mbp_mime_text, 1, mbp_mime_length, infile );
	if( bytes_read < mbp_mime_length ) {
		if( feof( infile ) ) {
			fprintf( stderr, "Error: unexpected end of file while reading header.\n" );
		} else {
			fprintf( stderr, "Error: file error while reading header.\n" );
		}
		abort();
	} else {
		mbp_mime_text[ mbp_mime_length ] = '\0';
		fprintf( stderr, "MIME type: %s\n", mbp_mime_text );
	}
	
	// description
	mbp_description_length = read_32be_int();
	mbp_description_text = malloc( mbp_description_length + 1 );
	if( mbp_description_text == NULL ){
		fprintf( stderr, "Error: memory allocation failed.\n" );
		abort();
	}
	bytes_read = fread( mbp_description_text, 1, mbp_description_length, infile );
	if( bytes_read < mbp_description_length ) {
		if( feof( infile ) ) {
			fprintf( stderr, "Error: unexpected end of file while reading header.\n" );
		} else {
			fprintf( stderr, "Error: file error while reading header.\n" );
		}
		abort();
	} else {
		mbp_description_text[ mbp_description_length ] = '\0';
		fprintf( stderr, "Description: %s\n", mbp_description_text );
	}
	
	// width and height
	mbp_width = read_32be_int();
	mbp_height = read_32be_int();
	fprintf( stderr, "Reported size: %dx%d\n", mbp_width, mbp_height );
	
	// color depth
	mbp_colordepth = read_32be_int();
	fprintf( stderr, "Color depth: %d\n", mbp_colordepth );
	
	// palette size
	mbp_palettesize = read_32be_int();
	fprintf( stderr, "Palette size: %d\n", mbp_palettesize );
	
	// picture binary data
	mbp_data_length = read_32be_int();
	fprintf( stderr, "Data size: %d bytes\n", mbp_data_length );
	mbp_data = malloc( mbp_data_length );
	if( mbp_data == NULL ){
		fprintf( stderr, "Error: memory allocation failed.\n" );
		abort();
	}
	bytes_read = fread( mbp_data, 1, mbp_data_length, infile );
	if( bytes_read < mbp_data_length ) {
		if( feof( infile ) ) {
			fprintf( stderr, "Warning: unexpected end of file while reading image data.\n" );
		} else {
			fprintf( stderr, "Warning: read error while reading image data.\n" );
		}
	}
	
	if( infile != stdin ) fclose( infile );
	
	// produce requested output
	switch( mode ){
		case 0:  fwrite(  mbp_data, mbp_data_length, 1, outfile ); break;
		case 1:  fprintf( outfile, "%d\n", mbp_type ); break;	
		case 2:  fprintf( outfile, "%s\n", mbp_type_description[ mbp_type ] ); break;		
		case 3:  fprintf( outfile, "%s\n", mbp_mime_text ); break;
		case 4:  fprintf( outfile, "%s\n", mbp_description_text ); break;
		default: fprintf( stderr,  "Error: invalid mode.\n" ); abort();
	}
	
	if( outfile != stdout ) fclose( outfile );
	
	return 0;
}


/*
	Reads a 32-bit big-endian unsigned integer from infile.
	infile needs to be already initialized.
	Aborts the program if reaches end-of-file or an error occurs.
	If no error occurs, the read value is converted to host endianness and returned.
*/
uint32_t read_32be_int(){
	
	size_t bytes_read;
	uint32_t value;
	
	bytes_read = fread( &value, 1, 4, infile );
	if( bytes_read < 4 ) {
		if( feof( infile ) ) {
			fprintf( stderr, "Error: unexpected end of file while reading header.\n" );
		} else {
			fprintf( stderr, "Error: file error while reading header.\n" );
		}
		abort();
	} else {
		value = be32toh( value );
		return value;
	}
	
}
