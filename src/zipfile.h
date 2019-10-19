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

typedef union zipfile_part {
    uint32_t signature;
    local_file_header_t local_file_header;
} zipfile_part_t;

#pragma pack(pop)


zip64_extended_information_t * get_zip64_extension (local_file_header_t* header);
uint64_t get_file_length (local_file_header_t  *header);
void patch_file_length (uint64_t offset, local_file_header_t  *header, uint64_t filesize);
uint64_t copy_file_data (uint64_t size, FILE* infile, FILE*outfile);

#endif
