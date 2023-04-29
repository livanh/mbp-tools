/*
	METADATA_BLOCK_PICTURE encoder: creates a METADATA_BLOCK_PICTURE structure from an
	image file, as defined in https://xiph.org/flac/format.html#metadata_block_picture

	Copyright 2016 Livanh <livanh@protonmail.com>

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
#include <string.h>

FILE *infile;
FILE *outfile;

uint32_t read_32be_int();
uint16_t read_16be_int();
uint8_t  read_8bit_int();
void     write_32be_int( int32_t data );

int main( int argc, char** argv ) {

	char  *infile_name = NULL;
	char  *outfile_name = NULL;
	size_t bytes_written;
	size_t bytes_read;
	uint16_t data_read;

	uint8_t  mbp_type = 0;              // type of picture (see help for possible values)
	char    *mbp_description_text = ""; // description of the image
	uint32_t mbp_width;                 // image width
	uint32_t mbp_height;                // image height
	uint8_t  n_components;              // number of channels in the image
	uint8_t  sample_precision;          // sample precision for each image channel
	uint8_t  mbp_palettesize;           // palette size (if image is a paletted PNG)
	char     mbp_mime_text[11];         // MIME type string of the image file
	uint32_t mbp_data_length;           // size of the image file in bytes
	char    *mbp_data;                  // image file data

	int c;
	opterr = 0;
	int help = 0;

	// process options
	while( ( c = getopt ( argc, argv, "t:c:o:h" ) ) != -1 )
		switch( c ) {
			case 't': mbp_type = atoi(optarg); break;
			case 'c': mbp_description_text = optarg; break;
			case 'o': outfile_name = optarg; break;
			case 'h': help = 1; break;
			case '?':
				if ( optopt == 't' || optopt == 'c' || optopt == 'o')
					fprintf ( stderr, "Error: option -%c requires an argument.\n", optopt);
				else if ( isprint( optopt ) )
					fprintf ( stderr, "Error: unknown option `-%c'.\n", optopt);
				else
					fprintf ( stderr, "Error: unknown option character `\\x%x'.\n", optopt);
				abort();
			default:
				abort();
		}

	if( help == 1 || argc == 1 ) {
		fprintf( stderr, "METADATA_BLOCK_PICTURE encoder\n" );
		fprintf( stderr, "Embeds an image file inside a METADATA_BLOCK_PICTURE structure\n" );
		fprintf( stderr, "Copyright 2016 Livanh <livanh@protonmail.com>\n" );
		fprintf( stderr, "This program is released under the GNU GPL v3 (http://www.gnu.org/licenses/)\n" );
		fprintf( stderr, "\n" );
		fprintf( stderr, "Usage: %s [<options>] <input file>\n", argv[0] );
		fprintf( stderr, "\n" );
		fprintf( stderr, "Available options:\n" );
		fprintf( stderr, " -o <output file>     choose output file (if missing, stdout is used)\n" );
		fprintf( stderr, " -t <type>            choose picture type (default is 0)\n" );
		fprintf( stderr, " -c <comment>         insert picture comment (optional)\n" );
		fprintf( stderr, " -h                   print this help\n" );
		fprintf( stderr, "\n" );
		fprintf( stderr, "Possible values for -t:\n" );
		fprintf( stderr, "    0:  Other\n" );
		fprintf( stderr, "    1:  32x32 pixel PNG file icon\n" );
		fprintf( stderr, "    2:  Other file icon\n" );
		fprintf( stderr, "    3:  Cover (front)\n" );
		fprintf( stderr, "    4:  Cover (back)\n" );
		fprintf( stderr, "    5:  Leaflet page\n" );
		fprintf( stderr, "    6:  Media (e.g. label side of CD)\n" );
		fprintf( stderr, "    7:  Lead artist/lead performer/soloist\n" );
		fprintf( stderr, "    8:  Artist/performer\n" );
		fprintf( stderr, "    9:  Conductor\n" );
		fprintf( stderr, "    10: Band/Orchestra\n" );
		fprintf( stderr, "    11: Composer\n" );
		fprintf( stderr, "    12: Lyricist/text writer\n" );
		fprintf( stderr, "    13: Recording Location\n" );
		fprintf( stderr, "    14: During recording\n" );
		fprintf( stderr, "    15: During performance\n" );
		fprintf( stderr, "    16: Movie/video screen capture\n" );
		fprintf( stderr, "    17: A bright coloured fish\n" );
		fprintf( stderr, "    18: Illustration\n" );
		fprintf( stderr, "    19: Band/artist logotype\n" );
		fprintf( stderr, "    20: Publisher/Studio logotype\n" );
		fprintf( stderr, "\n" );
		return 1;
	}

	if( optind == argc-1 ) { // 1 argument left: it's the input filename
		infile_name = argv[ optind ];
		fprintf( stderr, "Reading data from file %s\n", infile_name );
		infile = fopen( infile_name, "rb" );
		if( infile == NULL ) { 
			fprintf( stderr, "Error: cannot open input file.\n" );
			abort();
		}
	} else {
		fprintf( stderr, "Error: wrong number of arguments.\n" );
		abort();
	}

	// retrieve image info
	data_read = read_16be_int();
	if( data_read == 0xffd8 ){
		sprintf( mbp_mime_text, "image/jpeg" );
		fprintf( stderr, "JPEG file detected (%s)\n", mbp_mime_text );

		// look for APPLICATION or COMMENT blocks
		do {
			data_read = read_16be_int();
			if(
				(data_read >= 0xffe0 && data_read <= 0xffef) || // APP block
				data_read == 0xfffe // COM block
			){
				fprintf( stderr, "Found %x block ", data_read );
				data_read = read_16be_int();
				fprintf( stderr, "(%d bytes). Skipping\n", data_read );
				fseek( infile, data_read-2, SEEK_CUR );
			} else {
				break;
			}
		} while( 1 );
		
		// look for quantization tables
		if( data_read == 0xffdb ) {
			while( data_read == 0xffdb ){
				data_read = read_16be_int();
				fprintf( stderr, "Found quantization table (%d bytes). Skipping\n", data_read );
				fseek( infile, data_read-2, SEEK_CUR );
				data_read = read_16be_int();
			}
		}

		// look for start of frame marker
		if( data_read == 0xffc0 || data_read == 0xffc2 ){

			fprintf( stderr, "Found start-of-frame marker " );

			if( data_read == 0xffc0 )
				fprintf( stderr, "(type 0: baseline)\n" );
			else
				fprintf( stderr, "(type 2: progressive)\n" );

			read_16be_int(); // frame header length (not interesting here)

			sample_precision = read_8bit_int(); // sample precision
			if( sample_precision != 8 ) {
				fprintf( stderr, "Error: invalid sample precision (%d)\n", sample_precision );
				abort();
			} else {
				fprintf( stderr, "Sample precision: %d bits\n", sample_precision );
			}

			mbp_height = read_16be_int(); // image height
			mbp_width = read_16be_int(); // image width
			fprintf( stderr, "Image resolution: %dx%d pixels\n", mbp_width, mbp_height );

			n_components = read_8bit_int(); // number of components
			mbp_palettesize = 0;
			fprintf( stderr, "Number of components: %d\n", n_components );

			fseek( infile, 0, SEEK_END);
			mbp_data_length = ftell( infile );  // file size

		} else {
			fprintf( stderr, "Error: unsupported JPEG file format\n" );
			abort();
		}

	} else if( data_read == 0x8950 ) {
		if( 
			read_16be_int() == 0x4e47 &&
			read_16be_int() == 0x0d0a && 
			read_16be_int() == 0x1a0a &&
			read_32be_int() == 13 && // size of IHDR block
			read_32be_int() == 'I','H','D','R'
		) {
			sprintf( mbp_mime_text, "image/png" );
			fprintf( stderr, "PNG file detected (%s)\n", mbp_mime_text );

			mbp_width = read_32be_int();
			mbp_height = read_32be_int();
			fprintf( stderr, "Image resolution: %dx%d pixels\n", mbp_width, mbp_height );

			sample_precision = read_8bit_int(); // it might also be the palette size
												// the next read value will be used to decide
			data_read = read_8bit_int();  // color type
			switch( data_read ){
				case 0: // grayscale image
					n_components = 1;
					mbp_palettesize = 0;
					fprintf( stderr, "%d-bit grayscale image detected\n", sample_precision );
					break;
				case 2: // color RGB image
					n_components = 3;
					mbp_palettesize = 0;
					fprintf( stderr, "%d-bit RGB image detected\n", sample_precision );
					break;
				case 3: // color image with palette
					n_components = 0;
					mbp_palettesize = sample_precision;
					sample_precision = 0;
					fprintf( stderr, "%d-bit palette image detected\n", mbp_palettesize );
					break;
				case 4: // grayscale image with alpha channel
					n_components = 2;
					mbp_palettesize = 0;
					fprintf( stderr, "%d-bit grayscale+alpha image detected\n", sample_precision );
					break;
				case 6: // color RGB image with alpha channel
					n_components = 4;
					mbp_palettesize = 0;
					fprintf( stderr, "%d-bit RGB+alpha image detected\n", sample_precision );
					break;
				default:
					fprintf( stderr, "Error: invalid PNG color format detected\n" );
					abort();
			}

			fseek( infile, 0, SEEK_END);
			mbp_data_length = ftell( infile );  // file size

		} else {
			fprintf( stderr, "Error: unsupported image format\n" );
			abort();
		}
	} else {
		fprintf( stderr, "Error: unsupported image format\n" );
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

	// write data to output file

	write_32be_int( mbp_type );

	write_32be_int( strlen( mbp_mime_text ) );	
	bytes_written = fwrite( mbp_mime_text, 1, strlen( mbp_mime_text ), outfile );
	if( bytes_written < strlen( mbp_mime_text ) ) {
		fprintf( stderr, "Error: could not write to output file.\n" );
		abort();
	}

	write_32be_int( strlen( mbp_description_text ) );
	bytes_written = fwrite( mbp_description_text, 1, strlen( mbp_description_text ), outfile ); //FIXME: convert to UTF-8?
	if( bytes_written < strlen( mbp_description_text ) ) {
		fprintf( stderr, "Error: could not write to output file.\n" );
		abort();
	}

	write_32be_int( mbp_width );
	write_32be_int( mbp_height );
	write_32be_int( sample_precision * n_components );
	write_32be_int( mbp_palettesize );

	write_32be_int( mbp_data_length );
	mbp_data = malloc( mbp_data_length * sizeof(char) );
	if( mbp_data == NULL ){
		fprintf( stderr, "Error: memory allocation failed.\n" );
		abort();
	}
	fseek( infile, 0, SEEK_SET);
	bytes_read = fread( mbp_data, 1, mbp_data_length, infile );
	if( bytes_read < mbp_data_length ){
		fprintf( stderr, "Error: could not read input file.\n" );
		abort();
	}
	bytes_written = fwrite( mbp_data, 1, mbp_data_length, outfile );
	if( bytes_written < mbp_data_length ) {
		fprintf( stderr, "Error: could not write to output file.\n" );
		abort();
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

uint16_t read_16be_int(){
	
	size_t bytes_read;
	uint16_t value;
	
	bytes_read = fread( &value, 1, 2, infile );
	if( bytes_read < 2 ) {
		if( feof( infile ) ) {
			fprintf( stderr, "Error: unexpected end of file while reading header.\n" );
		} else {
			fprintf( stderr, "Error: file error while reading header.\n" );
		}
		abort();
	} else {
		value = be16toh( value );
		return value;
	}
	
}

uint8_t read_8bit_int(){
	size_t bytes_read;
	uint8_t value;

	bytes_read = fread( &value, 1, 1, infile );
	if( bytes_read < 1 ) {
		if( feof( infile ) ) {
			fprintf( stderr, "Error: unexpected end of file while reading header.\n" );
		} else {
			fprintf( stderr, "Error: file error while reading header.\n" );
		}
		abort();
	} else {
		return value;
	}
}

void write_32be_int( int32_t data ){
	size_t bytes_written;

	data = htobe32( data );
	bytes_written = fwrite( &data, 1, 4, outfile );
	if( bytes_written < 4 ) {
		fprintf( stderr, "Error: could not write to output file.\n" );
		abort();
	}
}
