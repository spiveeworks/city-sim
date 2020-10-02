#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define range(i, max) for (size_t i = 0; (i) < (max); ++(i))

#define rand_int(g) ((rand() % (2*(g)+1))-(g))

#define min(x, y) ((x) <= (y) ? (x) : (y))
#define max(x, y) ((x) >= (y) ? (x) : (y))

typedef int64_t num;
#define UNIT_CTIME (1ULL << 16U)
const num UNIT = UNIT_CTIME;
const num NUM_GREATEST = INT64_MAX;

// Newton-Raphson method of calculating 1/sqrt(x)
// (calling it fast invsqrt would be a lie while I have a while loop)
num invsqrt_nr(num x) {
	if (!x) x = 1;
	// scale^2 ~= UNIT / 2 ~= 1/sqrt(scrapqu)
	num y = UNIT;
	// @Performance count leading zeroes please
	while (x*y/UNIT*y/UNIT > 3 * UNIT / 2) {
		y = y * 5 / 7;
	}
	while (x*y/UNIT*y/UNIT < 2 * UNIT / 3) {
		y = y * 7 / 5;
	}
	y = y * (UNIT * 3 / 2 - x*y/UNIT*y/UNIT/2)/UNIT;
	y = y * (UNIT * 3 / 2 - x*y/UNIT*y/UNIT/2)/UNIT;
	y = y * (UNIT * 3 / 2 - x*y/UNIT*y/UNIT/2)/UNIT;
	if (x*y/UNIT*y/UNIT > 3 * UNIT / 2 || x*y/UNIT*y/UNIT < 2 * UNIT / 3) {
		printf("invsqrt(%ld) = %ld\n", x, y);
	}
	return y;
}

num num_hypot(num x, num y) {
	num qu = (x*x + y*y)/UNIT;
	return qu * invsqrt_nr(qu) / UNIT;
}

