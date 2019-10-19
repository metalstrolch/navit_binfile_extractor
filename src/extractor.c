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
#include <errno.h>

#include "zipfile.h"
#include "map.h"


static int filter_file(local_file_header_t * header, struct rect *r) {
    return 0;
}

static int64_t process_local_file(uint64_t offset, local_file_header_t  *header, FILE *infile, FILE *outfile,
                                  struct rect *r,
                                  local_file_header_t **stored_header) {
    *stored_header = NULL;
    int keep_zerofile =1;
    /* read rest of header */
    if(fread(((uint32_t *)(header)) +1, sizeof(*header) - sizeof(header->local_file_header_signature), 1, infile) > 0) {
        char * filename = NULL;
        uint64_t filesize;
        fprintf(stderr, "filename length %d, extra length %d\n", header->file_name_length, header->extra_field_length);
        *stored_header = (local_file_header_t*) malloc(sizeof(*header) + header->file_name_length + header->extra_field_length);
        /* copy the header */
        memcpy(*stored_header, header, sizeof(*header));
        /* read filename and extra fields*/
        filename = (char *)((*stored_header) +1);
        fread(filename, header->file_name_length + header->extra_field_length, 1, infile);

        /* get number of bytes to copy */
        filesize=get_file_length(*stored_header);

        /* filter file */
        if(filter_file(*stored_header, r)) {
            /* dump the data */
            copy_file_data(filesize, infile, NULL);
            if(keep_zerofile) {
                filesize = 0;
            } else {
                free(*stored_header);
                *stored_header = NULL;
                return 0;
            }
        }

        /* patch the new location and file size after filter */
        patch_file_length (offset, *stored_header, filesize);

        /* write out the new header */
        fwrite(*stored_header, sizeof(*header) + header->file_name_length + header->extra_field_length, 1, outfile);

        /* copy the compressed file */
        copy_file_data(filesize, infile, outfile);

        fprintf(stderr,"Filename %.*s, %ld\n", header->file_name_length, filename, filesize);
        /* done */
        return sizeof(*header) + header->file_name_length + header->extra_field_length + filesize;
    }
}

static int64_t process_central_directory_header(uint64_t offset, central_directory_header_t  *header, FILE *infile,
        FILE *outfile,
        struct rect *r,
        central_directory_header_t **stored_header) {
    /* read rest of header */
    if(fread(((uint32_t *)(header)) +1, sizeof(*header) - sizeof(header->central_file_header_signature), 1, infile) > 0) {
    }
    if(stored_header != NULL)
        *stored_header = NULL;
    return 0;
}

int process_binfile (FILE *infile, FILE* outfile, struct rect * r) {
    int64_t written=0;
    int64_t this_file;
    zipfile_part_t part;
    local_file_header_storage_t storage;

    memset(&storage, 0, sizeof(storage));
    storage.count =0;

    fprintf(stderr, "Extract area (%d, %d) - (%d, %d)\n", r->w.x, r->w.y, r->h.x, r->h.y);

    while (fread(&(part.signature), sizeof(part.signature), 1, infile) > 0) {
        local_file_header_t * file_header;
        switch(part.signature) {
        case LOCAL_FILE_HEADER_SIGNATURE:
            fprintf(stderr, "Got LOCAL FILE HEADER\n");
            this_file = process_local_file(written,&(part.local_file_header),infile, outfile, r, &file_header);
            if(file_header != NULL) {
                /* remember new file header and old written value */
                remember_local_file (&storage, file_header, written);
                written += this_file;
            }
            break;
        case CENTRAL_DIRECTORY_HEADER_SIGNATURE:
            fprintf(stderr, "Got CENTRAL DIRCTORY HEADER\n");
            process_central_directory_header(written, &(part.central_directory_header),infile, outfile, r, NULL);
        default:
            break;
        }
    }
    /* write central directory from the things we learned */
    written += write_central_directory(&storage, outfile);
    fprintf(stderr, "processed %ld files\n",storage.count);

    free_storage(&storage);
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

