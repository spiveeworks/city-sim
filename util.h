#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define array_ptr(T) struct {size_t len; (T) *data;}

#define range(i, max) for (size_t i = 0; (i) < (max); ++(i))

