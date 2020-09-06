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

