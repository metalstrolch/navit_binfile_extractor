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

#ifndef __zipfile_h
#define __zipfile_h
#include <stdint.h>

#pragma pack(push)
#pragma pack(1)

#define LOCAL_FILE_HEADER_SIGNATURE 0x04034b50
typedef struct local_file_header local_file_header_t;

struct local_file_header {
    uint32_t local_file_header_signature;
    uint16_t version_needed_to_extract;
    uint16_t general_purpose_bit_flag;
    uint16_t compressionmethod;
    uint16_t last_mod_file_time;
    uint16_t last_mod_file_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t file_name_length;
    uint16_t extra_field_length;
    /*file name string without 0*/
    /*extra field array */
};

typedef struct extra_field_header extra_field_header_t;
struct extra_field_header {
    uint16_t header_id;
    uint16_t data_size;
};

#define ZIP64_EXTENDED_INFORMATION_ID 0x0001
typedef struct zip64_extended_information zip64_extended_information_t;
struct zip64_extended_information {
    uint16_t header_id;
    uint16_t data_size;
    uint64_t uncompressed_size;
    uint64_t compressed_size;
    uint64_t offset;
    uint32_t disk_nr;
};

#define CENTRAL_DIRECTORY_HEADER_SIGNATURE 0x02014b50
typedef struct central_directory_header central_directory_header_t;
struct central_directory_header {
    uint32_t central_file_header_signature;
    uint16_t version_made_by;
    uint16_t version_needed_to_extract;
    uint16_t general_purpose_bit_flag;
    uint16_t compression_method;
    uint16_t last_mod_file_time;
    uint16_t last_mod_file_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t file_name_length;
    uint16_t extra_field_length;
    uint16_t file_comment_length;
    uint16_t disk_number_start;
    uint16_t internal_file_attributes;
    uint32_t external_file_attributes;
    uint32_t relative_offset_of_local_header;
    /*file name string without 0*/
    /*extra field array */
    /*file comment string without 0*/
};

#define END_OF_CENTRAL_DIR_64_SIGNATURE 0x06064b50
typedef struct end_of_central_dir_64 end_of_central_dir_64_t;
struct end_of_central_dir_64 {
    uint32_t end_of_central_dir_64_signature;
    uint64_t size_of_zip64_end_of_central_directory_record;
    uint16_t version_made_by;
    uint16_t version_needed_to_extract;
    uint32_t number_of_this_disk;
    uint32_t number_of_central_directory_disk;
    uint64_t central_directory_count_this_disk;
    uint64_t central_directory_count_total;
    uint64_t central_directory_size;
    uint64_t central_directory_offset;
    /*zip64 extensible data sector*/
};

#define ZIP64_END_OF_CENTRAL_DIR_LOCATOR_SIGNATURE 0x07064b50
typedef struct zip64_end_of_central_dir_locator zip64_end_of_central_dir_locator_t;
struct zip64_end_of_central_dir_locator {
    uint32_t zip64_end_of_central_dir_locator_signature;
    uint32_t number_of_central_directory_disk;
    uint64_t end_of_central_directory_offset;
    uint32_t total_number_of_disks;
};

#define END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
typedef struct end_of_central_dir end_of_central_dir_t;
struct end_of_central_dir {
    uint32_t end_of_central_dir_signature;
    uint16_t number_of_this_disk;
    uint16_t number_of_central_directory_disk;
    uint16_t central_directory_count_this_disk;
    uint16_t central_directory_count_total;
    uint32_t central_directory_size;
    uint32_t central_directory_offset;
    uint16_t file_comment_length;
    /*ZIP file comment       (variable size)*/
};

typedef union zipfile_part {
    uint32_t signature;
    local_file_header_t local_file_header;
    central_directory_header_t central_directory_header;
    end_of_central_dir_64_t end_of_central_dir_64;
    zip64_end_of_central_dir_locator_t zip64_end_of_central_dir_locator;
    end_of_central_dir_t end_of_central_dir;
} zipfile_part_t;
#pragma pack(pop)

typedef struct local_file_header_storage local_file_header_storage_t;
struct local_file_header_storage {
    local_file_header_t ** headers;
    uint64_t * offsets;
    uint64_t count;
};

zip64_extended_information_t * get_zip64_extension (local_file_header_t* header);
uint64_t get_file_length (local_file_header_t  *header);
void patch_file_length (uint64_t offset, local_file_header_t  *header, uint64_t filesize);
uint64_t copy_file_data (uint64_t size, FILE* infile, FILE*outfile);
uint64_t write_central_directory_entry(uint64_t offset, local_file_header_t * header, FILE* outfile);
uint64_t write_end_of_central_directory(uint64_t offset, uint64_t cd_offset, uint64_t size,
                                        local_file_header_storage_t *storage,
                                        FILE * outfile);

uint64_t write_central_directory(local_file_header_storage_t * storage, FILE *outfile);
void remember_local_file (local_file_header_storage_t  *storage, local_file_header_t * header, uint64_t offset);
void free_storage(local_file_header_storage_t  *storage);
#endif
