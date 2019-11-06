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
#include <math.h>

#include "map.h"

struct rect world_bbox = {
    { WORLD_BOUNDINGBOX_MIN_X, WORLD_BOUNDINGBOX_MIN_Y},
    { WORLD_BOUNDINGBOX_MAX_X, WORLD_BOUNDINGBOX_MAX_Y},
};

#if 0
void tile_bbox(char *tile, struct rect *r, int overlap) {
    struct coord c;
    int xo,yo;
    *r=world_bbox;
    while (*tile) {
        c.x=(r->l.x+r->h.x)/2;
        c.y=(r->l.y+r->h.y)/2;
        xo=(r->h.x-r->l.x)*overlap/100;
        yo=(r->h.y-r->l.y)*overlap/100;
        switch (*tile) {
        case 'a':
            r->l.x=c.x-xo;
            r->l.y=c.y-yo;
            break;
        case 'b':
            r->h.x=c.x+xo;
            r->l.y=c.y-yo;
            break;
        case 'c':
            r->l.x=c.x-xo;
            r->h.y=c.y+yo;
            break;
        case 'd':
            r->h.x=c.x+xo;
            r->h.y=c.y+yo;
            break;
        }
        tile++;
    }
}
#endif
void tile_bbox(char *tile, struct rect *r, int overlap) {
    struct coord c;
    int xo,yo;
    *r=world_bbox;
    while (*tile) {
        c.x=(r->l.x+r->h.x)/2;
        c.y=(r->l.y+r->h.y)/2;
        xo=(r->h.x-r->l.x)*overlap/100;
        yo=(r->h.y-r->l.y)*overlap/100;
        switch (*tile) {
        case 'a':
            if(*(tile +1)) {
                r->l.x=c.x;
                r->l.y=c.y;
            } else {
                r->l.x=c.x-xo;
                r->l.y=c.y-yo;
            }
            break;
        case 'b':
            if(*(tile +1)) {
                r->h.x=c.x;
                r->l.y=c.y;
            } else {
                r->h.x=c.x+xo;
                r->l.y=c.y-yo;
            }
            break;
        case 'c':
            if(*(tile +1)) {
                r->l.x=c.x;
                r->h.y=c.y;
            } else {
                r->l.x=c.x-xo;
                r->h.y=c.y+yo;
            }
            break;
        case 'd':
            if(*(tile +1)) {
                r->h.x=c.x;
                r->h.y=c.y;
            } else {
                r->h.x=c.x+xo;
                r->h.y=c.y+yo;
                break;
            }
        }
        tile++;
    }
}

int tile_len(char *tile) {
    int ret=0;
    while (tile[0] >= 'a' && tile[0] <= 'd') {
        tile++;
        ret++;
    }
    return ret;
}

/**
 * @brief check if rectangles overlap
 *
 * Ths function calculates if two rectangles overlap
 * @param[in] b1 - one rectangle
 * @param[in] b2 - another rectangle
 * @return 1 if they overlap, 0 otherwise
 */
int itembin_bbox_intersects (struct rect * b1, struct rect * b2) {
    // If one rectangle is on left side of other
    if (b1->l.x > b2->h.x || b2->l.x > b1->h.x)
        return 0;
    // If one rectangle is above other
    if (b1->h.y < b2->l.y || b2->h.y < b1->l.y)
        return 0;

    return 1;
}

/* navit uses slightly wrong erth radius. */
#define EARTHR 6371000.0
//#define EARTHR 6378137.0
/* requires math.h and libm */
void getmercator(double sx,double sy, double ex, double ey, struct rect * bbox) {
    sx = sx*EARTHR*M_PI/180;
    sy = log(tan(M_PI_4+sy*M_PI/360))*EARTHR;
    ex = ex*EARTHR*M_PI/180;
    ey = log(tan(M_PI_4+ey*M_PI/360))*EARTHR;
    bbox->l.x=round(sx);
    bbox->l.y=round(sy);
    bbox->h.x=round(ex);
    bbox->h.y=round(ey);
}
