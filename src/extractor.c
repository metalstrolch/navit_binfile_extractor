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
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "zipfile.h"
#include "map.h"

typedef struct extractor_parameters extractor_parameters_t;
struct extractor_parameters {
    struct rect area;
    double lat_bottom_left;
    double lon_bottom_left;
    double lat_top_right;
    double lon_top_right;
};

static void usage (void) {
    fprintf(stderr,"\n"
            " usage: navit_binfile_extractor [coordinates] \n"
            "\n"
            " NavIT binfile extractor extracts given area from a NavIT binfile\n"
            " It reads binfile from stdin and writes result to stdout. \n"
            "\n"
            " Coordinates\n"
            "  <bottom left lon> <bottom left lat> <top right lon> <top right lat>\n"
            "\n"
            " Example: extract Munich, Bavaria from world map\n"
            "  cat world.bin | navit_binfile_extractor 11.3 47.9 11.7 48.2 > munich.bin\n"
            "\n");
}

static int filter_file(local_file_header_t * header, struct rect *r) {
    char name[1024];
    struct rect bbox;
    /* zero terminate the name */
    memcpy(name, header +1, header->file_name_length);
    name[header->file_name_length]=0;

    tile_bbox(name, &bbox, 1);
    //fprintf(stderr,"%s -> (%d,%d)-(%d,%d)\n", name, bbox.l.x, bbox.l.y, bbox.h.x, bbox.h.y);
    if((itembin_bbox_intersects(r, &bbox)) || (tile_len(name) == 0)) {
        fprintf(stderr, "keep %s\n", name);
        return 0;
    } else
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
        //fprintf(stderr, "filename length %d, extra length %d\n", header->file_name_length, header->extra_field_length);
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

        //fprintf(stderr,"Filename %.*s, %ld\n", header->file_name_length, filename, filesize);
        /* done */
        return sizeof(*header) + header->file_name_length + header->extra_field_length + filesize;
    }
}

static int64_t process_central_directory_header(uint64_t offset, central_directory_header_t  *header, FILE *infile,
        FILE *outfile,
        struct rect *r,
        central_directory_header_t **stored_header) {

    *stored_header = NULL;
    /* read rest of header */
    if(fread(((uint32_t *)(header)) +1, sizeof(*header) - sizeof(header->central_file_header_signature), 1, infile) > 0) {
        char * filename = NULL;
        //fprintf(stderr, "filename length %d, extra length %d, comment length %d\n", header->file_name_length,
        //        header->extra_field_length, header->file_comment_length);
        *stored_header = (central_directory_header_t*) malloc(sizeof(*header) + header->file_name_length +
                         header->extra_field_length +
                         header->file_comment_length);
        /* copy the header */
        memcpy(*stored_header, header, sizeof(*header));
        /* read filename and extra fieldsi and file comment*/
        filename = (char *)((*stored_header) +1);
        fread(filename, header->file_name_length + header->extra_field_length + header->file_comment_length, 1, infile);

        //fprintf(stderr,"Filename %.*s\n", header->file_name_length, filename);
    }
    return 0; /* as we wrote nothing */
}

static int64_t process_end_of_central_dir_64(uint64_t offset, end_of_central_dir_64_t  *header, FILE *infile,
        FILE *outfile,
        struct rect *r,
        end_of_central_dir_64_t **stored_header) {

    *stored_header = NULL;
    /* read rest of header */
    if(fread(((uint32_t *)(header)) +1, sizeof(*header) - sizeof(header->end_of_central_dir_64_signature), 1, infile) > 0) {
        char * extra = NULL;
        uint64_t extra_length = (header->size_of_zip64_end_of_central_directory_record+12) - sizeof(*header);
        //fprintf(stderr, "extra length %ld\n", extra_length);
        *stored_header = (end_of_central_dir_64_t*) malloc(sizeof(*header) + extra_length);
        /* copy the header */
        memcpy(*stored_header, header, sizeof(*header));
        /* read extra field*/
        extra = (char *)((*stored_header) +1);
        fread(extra, extra_length, 1, infile);
    }
    return 0; /* as we wrote nothing */
}

static int64_t process_end_of_central_dir(uint64_t offset, end_of_central_dir_t  *header, FILE *infile,
        FILE *outfile,
        struct rect *r,
        end_of_central_dir_t **stored_header) {

    *stored_header = NULL;
    /* read rest of header */
    if(fread(((uint32_t *)(header)) +1, sizeof(*header) - sizeof(header->end_of_central_dir_signature), 1, infile) > 0) {
        char * comment = NULL;
        *stored_header = (end_of_central_dir_t*) malloc(sizeof(*header) + header->file_comment_length);
        /* copy the header */
        memcpy(*stored_header, header, sizeof(*header));
        /* read comment*/
        if(header->file_comment_length > 0) {
            comment = (char *)((*stored_header) +1);
            fread(comment, header->file_comment_length, 1, infile);
            //fprintf(stderr,"Comment %.*s\n", header->file_comment_length, comment);
        }
    }
    return 0; /* as we wrote nothing */
}

static int64_t process_zip64_end_of_central_dir_locator(uint64_t offset, zip64_end_of_central_dir_locator_t *header,
        FILE *infile,
        FILE *outfile,
        struct rect *r,
        zip64_end_of_central_dir_locator_t **stored_header) {

    *stored_header = NULL;
    /* read rest of header */
    if(fread(((uint32_t *)(header)) +1, sizeof(*header) - sizeof(header->zip64_end_of_central_dir_locator_signature), 1,
             infile) > 0) {
        *stored_header = (zip64_end_of_central_dir_locator_t*) malloc(sizeof(*header));
        /* copy the header */
        memcpy(*stored_header, header, sizeof(*header));
    }
    return 0; /* as we wrote nothing */
}

int process_binfile (FILE *infile, FILE* outfile, struct rect * r) {
    int64_t written=0;
    int64_t this_file;
    uint64_t central_directory_offset;
    uint64_t central_directory_size;
    zipfile_part_t part;
    local_file_header_storage_t storage;

    memset(&storage, 0, sizeof(storage));
    storage.count =0;

    while (fread(&(part.signature), sizeof(part.signature), 1, infile) > 0) {
        local_file_header_t * file_header;
        central_directory_header_t * central_directory_header;
        zip64_end_of_central_dir_locator_t * zip64_end_of_central_dir_locator;
        end_of_central_dir_64_t * end_of_central_dir_64;
        end_of_central_dir_t * end_of_central_dir;
        switch(part.signature) {
        case LOCAL_FILE_HEADER_SIGNATURE:
            //fprintf(stderr, "Got LOCAL FILE HEADER\n");
            this_file = process_local_file(written,&(part.local_file_header),infile, outfile, r, &file_header);
            if(file_header != NULL) {
                /* remember new file header and old written value */
                remember_local_file (&storage, file_header, written);
                written += this_file;
            }
            break;
        case CENTRAL_DIRECTORY_HEADER_SIGNATURE:
            //fprintf(stderr, "Got CENTRAL DIRCTORY HEADER\n");
            process_central_directory_header(written, &(part.central_directory_header),infile, outfile, r,
                                             &central_directory_header);
            if(central_directory_header != NULL)
                free(central_directory_header);
            break;
        case END_OF_CENTRAL_DIR_64_SIGNATURE:
            //fprintf(stderr, "Got ZIP64 END OF CENTRAL DIRECTORY\n");
            process_end_of_central_dir_64(written, &(part.end_of_central_dir_64),infile, outfile, r,
                                          &end_of_central_dir_64);
            if(end_of_central_dir_64 != NULL)
                free(end_of_central_dir_64);
            break;
        case ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIGNATURE:
            //fprintf(stderr, "Got ZIP64 CENTRAL DIECTORY LOCATOR\n");
            process_zip64_end_of_central_dir_locator(written, &(part.zip64_end_of_central_dir_locator),infile, outfile, r,
                    &zip64_end_of_central_dir_locator);
            if(zip64_end_of_central_dir_locator != NULL)
                free(zip64_end_of_central_dir_locator);
            break;
        case END_OF_CENTRAL_DIR_SIGNATURE:
            //fprintf(stderr, "Got END OF CENTRAL DIRECTORY\n");
            process_end_of_central_dir(written, &(part.end_of_central_dir),infile, outfile, r,
                                       &end_of_central_dir);
            if(end_of_central_dir != NULL)
                free(end_of_central_dir);
            break;
        default:
            //fprintf(stderr, "Got unknown header %x\n", part.signature);
            break;
        }
    }
    /* write central directory from the things we learned */
    central_directory_offset = written;
    central_directory_size = write_central_directory(&storage, outfile);
    written += central_directory_size;
    /* write end of central directory structures */
    written += write_end_of_central_directory(written, central_directory_offset, central_directory_size, &storage, outfile);
    fprintf(stderr, "processed %ld files\n",storage.count);

    free_storage(&storage);
    return 0;
}



int main (int argc, char ** argv) {
    int option_index=1;
    FILE * infile = stdin;
    FILE * outfile = stdout;
    extractor_parameters_t p;
    char * endp;

    fprintf(stderr, "NavIT binfile extractor \n"
            "Created by Metalstrolch 2019 \n");

    if((argc - option_index) != 4) {
        usage();
        exit(1);
    }

    p.lon_bottom_left = strtod(argv[option_index], &endp);
    if(endp != (argv[option_index] + strlen(argv[option_index]))) {
        usage();
        exit(1);
    }
    p.lat_bottom_left = strtod(argv[option_index +1], &endp);
    if(endp != (argv[option_index +1] + strlen(argv[option_index +1]))) {
        usage();
        exit(1);
    }
    p.lon_top_right = strtod(argv[option_index +2], &endp);
    if(endp != (argv[option_index +2] + strlen(argv[option_index +2]))) {
        usage();
        exit(1);
    }
    p.lat_top_right = strtod(argv[option_index +3], &endp);
    if(endp != (argv[option_index +3] + strlen(argv[option_index +3]))) {
        usage();
        exit(1);
    }
    /* same order as the filename from planet extractor */
    getmercator(p.lon_bottom_left,p.lat_bottom_left,p.lon_top_right,p.lat_top_right, &p.area);

    fprintf(stderr, "Extract area (lon %f, lat %f) - (lon %f, lat %f)\n",p.lon_bottom_left, p.lat_bottom_left,
            p.lon_top_right, p.lat_top_right);
    fprintf(stderr, "NavIT Mercator (%d, %d) - (%d, %d)\n", p.area.l.x, p.area.l.y, p.area.h.x, p.area.h.y);
    return process_binfile (infile, outfile, &p.area);
}

