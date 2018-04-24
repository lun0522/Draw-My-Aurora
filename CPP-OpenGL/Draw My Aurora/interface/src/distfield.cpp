//
//  distfield.cpp
//  Draw My Aurora
//
//  Created by Pujun Lun on 4/23/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "distfield.hpp"

inline DistanceField::Point DistanceField::get(Grid &g, int x, int y) {
    return g.points[(y + 1) * gridWidth + (x + 1)];
}

inline void DistanceField::put(Grid &g, int x, int y, const Point &p) {
    g.points[(y + 1) * gridWidth + (x + 1)] = p;
}

void DistanceField::loadImage(int width, int height, unsigned char* image) {
    imageWidth = width;
    imageHeight = height;
    gridWidth = imageWidth + 2;
    gridHeight = imageHeight + 2;
    numPoint = gridWidth * gridHeight; // include padding
    grid1.points = (Point *)malloc(numPoint * sizeof(Point));
    grid2.points = (Point *)malloc(numPoint * sizeof(Point));
    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            if (image[y * imageWidth + x] < 128) {
                put(grid1, x, y, inside);
                put(grid2, x, y, empty);
            } else {
                put(grid2, x, y, inside);
                put(grid1, x, y, empty);
            }
        }
    }
    for (int x = 0; x < imageWidth; ++x) { // top and buttom padding
        put(grid2, x, -1, get(grid2, x, 0));
        put(grid1, x, -1, get(grid1, x, 0));
        put(grid2, x, imageHeight, get(grid2, x, imageHeight - 1));
        put(grid1, x, imageHeight, get(grid1, x, imageHeight - 1));
    }
    for (int y = -1; y <= imageHeight; ++y) { // left and right padding
        put(grid2, -1, y, get(grid2, 0, y));
        put(grid1, -1, y, get(grid1, 0, y));
        put(grid2, imageWidth, y, get(grid2, imageWidth - 1, y));
        put(grid1, imageWidth, y, get(grid1, imageWidth - 1, y));
    }
}

inline void DistanceField::groupCompare(Grid &g, int x, int y, const __m256i& offsets) {
    Point p = get(g, x, y);
    
    /* Point other = Get( g, x+offsetx, y+offsety ); */
    int *offsetsPtr = (int *)&offsets;
    Point pn[4] = {
        get(g, x + offsetsPtr[0], y + offsetsPtr[4]),
        get(g, x + offsetsPtr[1], y + offsetsPtr[5]),
        get(g, x + offsetsPtr[2], y + offsetsPtr[6]),
        get(g, x + offsetsPtr[3], y + offsetsPtr[7]),
    };
    
    /* other.dx += offsetx; other.dy += offsety; */
    __m256i *pnPtr = (__m256i *)pn;
    // x0, y0, x1, y1, x2, y2, x3, y3 -> x0, x1, x2, x3, y0, y1, y2, y3
    static const __m256i mask = _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7);
    __m256i vecCoords = _mm256_permutevar8x32_epi32(*pnPtr, mask);
    vecCoords = _mm256_add_epi32(vecCoords, offsets);
    
    /* other.DistSq() */
    int *coordsPtr = (int *)&vecCoords;
    // note that _mm256_mul_epi32 only applies on the lower 128 bits
    __m256i vecPermuted = _mm256_permute2x128_si256(vecCoords , vecCoords, 1);
    __m256i vecSqrDists = _mm256_add_epi64(_mm256_mul_epi32(vecCoords, vecCoords),
                                           _mm256_mul_epi32(vecPermuted, vecPermuted));
    
    /* if (other.DistSq() < p.DistSq()) p = other; */
    int64_t prevDist = p.distSq(), index = -1;
    for (int i = 0; i < 4; ++i) {
        int64_t dist = *((int64_t *)&vecSqrDists + i);
        if (dist < prevDist) {
            prevDist = dist;
            index = i;
        }
    }
    if (index != -1) put(g, x, y, { coordsPtr[index], coordsPtr[index + 4] });
}

inline DistanceField::Point DistanceField::singleCompare(Grid &g, int x, int y, int offsetx, int offsety, Point other) {
    Point self = get(g, x, y);
    other.dx += offsetx;
    other.dy += offsety;
    
    if (other.distSq() < self.distSq()) {
        put(g, x, y, other);
        return other;
    } else {
        return self;
    }
}

void DistanceField::generateSDF(Grid &g) {
    // Pass 0
    static const __m256i offsets0 = _mm256_setr_epi32(-1, -1, 0, 1, 0, -1, -1, -1);
    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x)
            groupCompare(g, x, y, offsets0);
        
        Point prev = empty;
        for (int x = imageWidth - 1; x >= 0; --x)
            prev = singleCompare(g, x, y, 1, 0, prev);
    }
    
    // Pass 1
    static const __m256i offsets1 = _mm256_setr_epi32(1, -1, 0, 1, 0, 1, 1, 1);
    for (int y = imageHeight - 1; y >= 0; --y) {
        for (int x = imageWidth - 1; x >= 0; --x)
            groupCompare(g, x, y, offsets1);
        
        Point prev = empty;
        for (int x = 0; x < imageWidth; ++x)
            prev = singleCompare(g, x, y, -1, 0, prev);
    }
}

DistanceField::~DistanceField() {
    if (grid1.points) free(grid1.points);
    if (grid2.points) free(grid2.points);
}