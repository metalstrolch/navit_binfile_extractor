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

static zip64_extended_information_t * get_zip64_extension (local_file_header_t* header) {
    char * extra = NULL;
    uint64_t used =0;
    /* get the pointer to the extra fields */
    extra = ((char *)(header +1)) +header->file_name_length;
    if(header->extra_field_length > 0) {
        fprintf(stderr, "got %d extra bytes\n", header->extra_field_length);
        while(used < header->extra_field_length) {
            zip64_extended_information_t *zip64 = (zip64_extended_information_t*)(extra + used);
            if(zip64->header_id == ZIP64_EXTENDED_INFORMATION_ID) {
                fprintf(stderr, "zip64 extended header found\n");
                return zip64;
            } else {
                used += zip64->data_size + sizeof(zip64->header_id);
            }
        }
    }
    return NULL;
}

static uint64_t get_file_length (local_file_header_t  *header) {
    uint64_t filesize =0;
    zip64_extended_information_t * zip64_extended = NULL;
    /* check for 64 bit extension */
    zip64_extended = get_zip64_extension(header);
    /* get the file size */
    if(zip64_extended != NULL) {
        /* get the file size */
        filesize=zip64_extended->compressed_size;
    } else {
        /* get the file size */
        filesize = header->compressed_size;
    }
    return filesize;
}

static void patch_file_length (uint64_t offset, local_file_header_t  *header, uint64_t filesize) {
    zip64_extended_information_t * zip64_extended = NULL;
    /* check for 64 bit extension */
    zip64_extended = get_zip64_extension(header);
    /* patch the header with new file size */
    if(zip64_extended != NULL) {
        /* be paranoid and flag usage of 64 bit in file header */
        header->compressed_size = 0xFFFFFFFF;
        header->uncompressed_size = 0xFFFFFFFF;
        /* patch in the new offset of this file */
        zip64_extended->offset=offset;
        /* get the file size */
        zip64_extended->compressed_size=filesize;
        if(filesize == 0)
            zip64_extended->uncompressed_size=0;
    } else {
        /* patch the file size */
        header->compressed_size=filesize;
        if(filesize == 0)
            header->uncompressed_size=0;
    }
}

static uint64_t copy_file_data (uint64_t size, FILE* infile, FILE*outfile) {
    uint64_t bsize = 10*1024*1024;
    uint64_t copied = 0;
    char * buffer = malloc(bsize);

    while(copied < size) {
        uint64_t to_read = size;
        if(to_read > bsize)
            to_read = bsize;
        errno = 0;
        if(fread(buffer, 1, to_read, infile) != to_read) {
            fprintf(stderr, "ERROR reading: %s\n", strerror(errno));
            free(buffer);
            return -1;
        } else {
            errno=0;
            if(outfile != NULL) {
                if(fwrite(buffer, 1, to_read, outfile) != to_read) {
                    fprintf(stderr, "ERROR writing: %s\n", strerror(errno));
                    free(buffer);
                    return -1;
                }
            }
            copied +=to_read;
        }
    }
    free(buffer);
    return size;
}

typedef struct local_file_header_storage local_file_header_storage_t;
struct local_file_header_storage {
    local_file_header_t ** headers;
    uint64_t * offsets;
    uint64_t count;
};

static void remember_local_file (local_file_header_storage_t  *storage, local_file_header_t * header, uint64_t offset) {
    storage->headers = reallocarray(storage->headers, storage->count +1, sizeof(local_file_header_t*));
    storage->offsets = reallocarray(storage->offsets, storage->count +1, sizeof(uint64_t));
    storage->headers[storage->count] = header;
    storage->offsets[storage->count] = offset;
    storage->count ++;
}

static void free_storage(local_file_header_storage_t  *storage) {
    uint64_t i;
    for(i = 0; i < storage->count; i ++) {
        if(storage->headers[i] != NULL)
            free(storage->headers[i]);
    }
    if(storage->headers != NULL)
        free(storage->headers);
    if(storage->offsets != NULL)
        free(storage->offsets);
}

static int filter_file(local_file_header_t * header, struct rect *r) {
    return 1;
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
            this_file= process_local_file(written,&(part.local_file_header),infile, outfile, r, &file_header);
            if(file_header != NULL) {
                /* remember new file header and old written value */
                remember_local_file (&storage, file_header, written);
                written += this_file;
            }
            break;
        default:
            break;
        }
    }
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

