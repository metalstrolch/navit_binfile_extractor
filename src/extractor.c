/*
 * navit_binfile_extractor - a tool to extract smaller regions out of
 * ready made Navit binfiles
 * Copyright (C) 2005-2019 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>

#include "zipfile.h"
#include "map.h"

static int64_t process_local_file(local_file_header_t  *header, FILE *infile, FILE *outfile, struct rect *r, local_file_header_t **stored_header) {
    *stored_header = NULL;
    /* read rest of header */
    if(fread(((uint32_t *)(header)) +1, sizeof(*header) - sizeof(header->local_file_header_signature), 1, infile) > 0) {
        char * filename = NULL;
        char * extra = NULL;
	uint64_t filesize;
	int have_64=0;
        fprintf(stderr, "filename length %d, extra length %d\n", header->file_name_length, header->extra_field_length);
        *stored_header = (local_file_header_t*) malloc(sizeof(*header) + header->file_name_length + header->extra_field_length);
        filesize = header->compressed_size;
	/* copy the header */
	memcpy(*stored_header, header, sizeof(*header));
	/* read filename and extra fields*/
	filename = (char *)((*stored_header) +1);
	fread(filename, header->file_name_length + header->extra_field_length, 1, infile);
	fprintf(stderr,"Filename %.*s, %ld\n", header->file_name_length, filename, filesize);
	/* get the pointer to the extra fields */
	extra =filename + header->file_name_length;
	if(header->extra_field_length > 0) {
	    fprintf(stderr, "got %d extra bytes\n", header->extra_field_length);
	}

	/* write out the new header */
	fwrite(*stored_header, sizeof(*header) + header->file_name_length + header->extra_field_length, 1, outfile);
	/* copy the vompressed file */

        /* patch the new size */
	if(have_64) {
	   (*stored_header)->compressed_size = 0xFFFFFFFF;
	   (*stored_header)->uncompressed_size = 0xFFFFFFFF;
	}
	else {
	   (*stored_header)->compressed_size = filesize;
	   if(filesize == 0)
	       (*stored_header)->uncompressed_size = 0;
	}
	/* done */
	return sizeof(*header) + header->file_name_length + header->extra_field_length + filesize;
    }
}

int process_binfile (FILE *infile, FILE* outfile, struct rect * r) {
    int64_t written=0;
    int64_t this_file;
    int64_t file_count=0;
    zipfile_part_t part;
    fprintf(stderr, "Extract area (%d, %d) - (%d, %d)\n", r->w.x, r->w.y, r->h.x, r->h.y);

    while (fread(&(part.signature), sizeof(part.signature), 1, infile) > 0) {
	local_file_header_t * file_header;
        switch(part.signature) {
	    case LOCAL_FILE_HEADER_SIGNATURE:
	        fprintf(stderr, "Got LOCAL FILE HEADER\n");
		this_file= process_local_file(&(part.local_file_header),infile, outfile, r, &file_header);
		if(file_header != NULL) {
		    /* remember new file header and old written value */
		    written += this_file;
		    file_count ++;
		}
	        break;
	    default:
	        break;
	}
    }
    fprintf(stderr, "processed %ld files\n",file_count);
    return 0;
}



int main (int argc, char ** argv) {
    FILE * infile = stdin;
    FILE * outfile = stdout;
    struct rect r = {{0,0},{1,1}};

    fprintf(stderr, "NavIT binfile extractor \n"
                    "Created by Metalstrolch 2019 \n");
    return process_binfile (infile, outfile, &r); 
}

