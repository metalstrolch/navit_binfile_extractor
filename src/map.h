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

#ifndef __map_h
#define __map_h
#include <stdint.h>

#define WORLD_BOUNDINGBOX_MIN_X -20000000
#define WORLD_BOUNDINGBOX_MAX_X  20000000
#define WORLD_BOUNDINGBOX_MIN_Y -20000000
#define WORLD_BOUNDINGBOX_MAX_Y  20000000

struct coord {
    int x;
    int y;
};


struct rect {
    struct coord l;
    struct coord h;
};

void tile_bbox(char *tile, struct rect *r, int overlap);
int tile_len(char *tile);
int itembin_bbox_intersects (struct rect * b1, struct rect * b2);
void getmercator(double sx,double sy, double ex, double ey, struct rect * bbox);
#endif
