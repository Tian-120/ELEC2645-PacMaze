#ifndef PACMAZEMAP_H
#define PACMAZEMAP_H

#include "Utils.h"

/* ===== WALLS ===== */
#define WALL_COUNT 22

static const AABB walls[WALL_COUNT] = {
    /* Outer border walls */
    {0,   30, 240, 6},
    {0,  234, 240, 6},
    {0,   30, 6,   210},
    {234, 30, 6,   210},

    /* Inner maze walls */
    {24, 54, 192, 6},
    {210,  54,   6,  54},
    {192,  78,  24,   6},
    { 24,  90, 150,   6},
    {108,  78,   6,  18},
    { 24,  90,   6,  96},
    { 24, 126,  96,   6},
    {138, 114,   6,  60},
    {174, 114,   6,  78},
    {204, 138,  30,   6},
    { 54, 162,  24,   6},
    { 72, 156,   6,  54},
    {108, 168,   6,  66},
    {138, 198,   6,  36},
    {174, 186,  42,   6},
    { 42, 192,   6,  24},
    {  6, 174,  24,   6},
    {174, 210,  42,   6}
};

/* ===== PELLETS ===== */
/* Grid positions based on your 6x6 internal map system */
typedef struct {
    int gx;
    int gy;
} PelletGridPos;

#define PELLET_COUNT 12

static const PelletGridPos pellet_grid[PELLET_COUNT] = {
    {2, 2},
    {8, 2},
    {14, 2},
    {20, 2},
    {26, 2},
    {32, 2},
    {5, 10},
    {10, 18},
    {16, 24},
    {23, 12},
    {30, 20},
    {34, 28}
};

#endif