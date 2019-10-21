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

#ifndef NAVIT_COMPATIBLE
#define NAVIT_COMPATIBLE 1
#endif

zip64_extended_information_t * get_zip64_extension (local_file_header_t* header) {
    char * extra = NULL;
    uint64_t used =0;
    /* get the pointer to the extra fields */
    extra = ((char *)(header +1)) +header->file_name_length;
    if(header->extra_field_length > 0) {
        while(used < header->extra_field_length) {
            zip64_extended_information_t *zip64 = (zip64_extended_information_t*)(extra + used);
            if(zip64->header_id == ZIP64_EXTENDED_INFORMATION_ID) {
                //fprintf(stderr, "zip64 extended header found\n");
                return zip64;
            } else {
                used += sizeof(extra_field_header_t) + zip64->data_size;
            }
        }
    }
    return NULL;
}

uint64_t get_file_length (local_file_header_t  *header) {
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

void patch_file_length (uint64_t offset, local_file_header_t  *header, uint64_t filesize) {
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
    if(filesize == 0) {
        /* patch compression method and crc */
        header->crc32 = 0;
        header->compressionmethod=0;

    }
}

uint64_t copy_file_data (uint64_t size, FILE* infile, FILE*outfile) {
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

uint64_t write_central_directory_entry(uint64_t offset, local_file_header_t * header, FILE* outfile) {
    uint16_t extended_size;
    void * extended;
    central_directory_header_t entry;
#if NAVIT_COMPATIBLE
    zip64_extended_information_old_t zip64_old;
#else
    zip64_extended_information_t zip64;
    zip64_extended_information_t * header_zip64;
#endif

#if NAVIT_COMPATIBLE
    extended=&zip64_old;
    extended_size = sizeof(zip64_old);
#else
    extended=&zip64;
    extended_size = sizeof(zip64);
    header_zip64 = get_zip64_extension (header);
#endif


    memset(&entry, 0, sizeof(entry));
    entry.central_file_header_signature=CENTRAL_DIRECTORY_HEADER_SIGNATURE;
    entry.version_made_by=0x031e; /* UNIX, spec 3.0 */
    entry.version_needed_to_extract = 0x002d; /* version 4.5 for zip64*/
    entry.general_purpose_bit_flag = header->general_purpose_bit_flag;
    entry.compression_method = header->compressionmethod;
    entry.last_mod_file_time = header->last_mod_file_time;
    entry.last_mod_file_date = header->last_mod_file_date;
    entry.crc32 = header->crc32;
    entry.compressed_size = 0xFFFFFFFF;
    entry.uncompressed_size = 0xFFFFFFFF;
    entry.compressed_size = header->compressed_size;
    entry.uncompressed_size = header->uncompressed_size;
    entry.file_name_length = header->file_name_length;
    entry.extra_field_length = extended_size;
    entry.file_comment_length = 0;
    entry.disk_number_start = 0;
    entry.internal_file_attributes = 0;
    entry.external_file_attributes = 0;
    entry.relative_offset_of_local_header=0xFFFFFFFF;
#if NAVIT_COMPATIBLE
    memset(&zip64_old, 0, sizeof(zip64_old));
    zip64_old.header_id=ZIP64_EXTENDED_INFORMATION_ID;
    zip64_old.data_size=sizeof(zip64_old) - sizeof(extra_field_header_t);
    zip64_old.offset=offset;
#else
    memset(&zip64, 0, sizeof(zip64));
    zip64.header_id=ZIP64_EXTENDED_INFORMATION_ID;
    zip64.data_size=sizeof(zip64) - sizeof(extra_field_header_t);
    if(header_zip64 == NULL) {
        zip64.uncompressed_size=header->uncompressed_size;
        zip64.compressed_size=header->compressed_size;
        zip64.offset=offset;
        zip64.disk_nr=0;
    } else {
        zip64.uncompressed_size=header_zip64->uncompressed_size;
        zip64.compressed_size=header_zip64->compressed_size;
        zip64.offset=offset;
        zip64.disk_nr=0;
    }
#endif

    fwrite(&entry, sizeof(entry), 1, outfile);
    fwrite(header +1, header->file_name_length,1, outfile);
    fwrite(extended, extended_size, 1, outfile);
    return (sizeof(entry) + header->file_name_length + extended_size);
}

uint64_t write_central_directory(local_file_header_storage_t * storage, FILE *outfile) {
    uint64_t written =0;
    uint64_t a;
    for(a =0; a < storage->count; a ++) {
        written += write_central_directory_entry(storage->offsets[a], storage->headers[a], outfile);
    }
    return written;
}

uint64_t write_end_of_central_directory(uint64_t offset, uint64_t cd_offset, uint64_t size,
                                        local_file_header_storage_t *storage,
                                        FILE * outfile) {
    uint64_t written = 0;
#if !NAVIT_COMPATIBLE
    const char * message="created by NavIT binfile extractor";
#endif
    end_of_central_dir_64_t eoc64;
    zip64_end_of_central_dir_locator_t eoc64_locator;
    end_of_central_dir_t eoc;

    memset (&eoc64, 0, sizeof(eoc64));
    memset (&eoc64_locator, 0, sizeof(eoc64_locator));
    memset (&eoc, 0, sizeof(eoc));

    eoc64.end_of_central_dir_64_signature = END_OF_CENTRAL_DIR_64_SIGNATURE;
    eoc64.size_of_zip64_end_of_central_directory_record = sizeof(eoc64) - 12;
    eoc64.version_made_by = 0x031e; /* UNIX, spec 3.0 */
    eoc64.version_needed_to_extract = 0x002d; /* version 4.5 for zip64*/
    eoc64.number_of_this_disk = 0;
    eoc64.number_of_central_directory_disk = 0;
    eoc64.central_directory_count_this_disk = storage->count;
    eoc64.central_directory_count_total = storage->count;
    eoc64.central_directory_size = size;
    eoc64.central_directory_offset = cd_offset;

    fwrite(&eoc64, sizeof(eoc64), 1, outfile);
    written += sizeof(eoc64);

    eoc64_locator.zip64_end_of_central_dir_locator_signature = ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIGNATURE;
    eoc64_locator.number_of_central_directory_disk = 0;
    eoc64_locator.end_of_central_directory_offset = offset;
    eoc64_locator.total_number_of_disks = 1;

    fwrite(&eoc64_locator, sizeof(eoc64_locator), 1, outfile);
    written += sizeof(eoc64_locator);

    eoc.end_of_central_dir_signature = END_OF_CENTRAL_DIR_SIGNATURE;
    eoc.number_of_this_disk = 0;
    eoc.number_of_central_directory_disk = 0;
    eoc.central_directory_count_this_disk = 0xFFFF;
    eoc.central_directory_count_total = 0xFFFF;
    eoc.central_directory_size = 0xFFFFFFFF;
    eoc.central_directory_offset = 0xFFFFFFFF;
    eoc.file_comment_length = 0;
#if !NAVIT_COMPATIBLE
    /* NavIT doesn't support zip file messages. It even crashes if one is there */
    eoc.file_comment_length = strlen(message);
#endif

    fwrite(&eoc, sizeof(eoc), 1, outfile);
    written += sizeof(eoc);
#if !NAVIT_COMPATIBLE
    fwrite(message, strlen(message), 1, outfile);
    written += strlen(message);
#endif

    return written;
}

void remember_local_file (local_file_header_storage_t  *storage, local_file_header_t * header, uint64_t offset) {
    storage->headers = reallocarray(storage->headers, storage->count +1, sizeof(local_file_header_t*));
    storage->offsets = reallocarray(storage->offsets, storage->count +1, sizeof(uint64_t));
    storage->headers[storage->count] = header;
    storage->offsets[storage->count] = offset;
    storage->count ++;
}

void free_storage(local_file_header_storage_t  *storage) {
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
