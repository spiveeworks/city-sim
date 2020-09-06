#pragma once

#include "util.h"
#include "data.h"

int frame = 0;

typedef int64_t num;
#define UNIT_CTIME (1ull << 16)
const num UNIT = UNIT_CTIME;

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


// @Robustness why do large IDIM values cause a segfault?
#define IDIM 300
#define DIM_CTIME (UNIT_CTIME * IDIM)
const num DIM = DIM_CTIME;

#define CHAR_CAP (IDIM * IDIM / 4)
#define CHAR_INITIAL 10

#define REACH (UNIT)
#define MAX_HEALTH 60

size_t char_num = 0;

struct Char {
	num x, y;
	// num velx, vely;
	// int deadframe;
	Recipe goal;
	num craft_x, craft_y;
	int craft_t;
	uint8_t input_count;
	long inputs[RECIPE_INPUT_CAP];
	long held_item;
} chars[CHAR_CAP];

#define ITEM_CAP (IDIM * IDIM / 16)

size_t item_num = 0;

struct Item {
	num x, y;
	long held_by;
	int change_frame;
	ItemType type;
} items[ITEM_CAP];

size_t fixture_num = 0;

#define FIXTURE_CAP (IDIM * IDIM / 4)
enum FixtureType {
	FIXTURE_NULL,
	FIXTURE_FACTORY,
};

struct Fixture {
	num x, y;
	enum FixtureType type;
	int change_frame;
} fixtures[FIXTURE_CAP];

#define AWARENESS (UNIT_CTIME * 32)

#define CHUNK_SIZE (UNIT_CTIME * 16)
#define CHUNK_DIM (2 * DIM_CTIME / CHUNK_SIZE + 1)
#define CHUNK_BUFFER_SIZE 1024

typedef uint32_t ref;
#define REF_SHIFT 30
const ref REF_CHAR = 1u<<REF_SHIFT;
const ref REF_ITEM = 2u<<REF_SHIFT;
const ref REF_FIXTURE = 3u<<REF_SHIFT;
const ref REF_SORT = ~0u<<REF_SHIFT;
const ref REF_IND = ~(~0u<<REF_SHIFT);

struct Chunk {
	long total_num;
	ref refs[CHUNK_BUFFER_SIZE];
} chunks[CHUNK_DIM][CHUNK_DIM];

#define get_chunk(x) (((x)+DIM)/CHUNK_SIZE)

bool chunk_remove(struct Chunk *chunk, ref r) {
	range (i, chunk->total_num) {
		if (chunk->refs[i] == r) {
			chunk->total_num--;
			chunk->refs[i] = chunk->refs[chunk->total_num];
			chunk->refs[chunk->total_num] = 0;
			return true;
		}
	}
	return false;
}

void chunk_remove_char(long i) {
	struct Char c = chars[i];
	size_t ci = get_chunk(c.x);
	size_t cj = get_chunk(c.y);
	if (!chunk_remove(&chunks[ci][cj], i | REF_CHAR)) {
		printf("WARNING: char %ld not removed from chunk %lu, %lu\n", i, ci, cj);
	}
}

void chunk_remove_item(long i) {
	struct Item c = items[i];
	size_t ci = get_chunk(c.x);
	size_t cj = get_chunk(c.y);
	if (!chunk_remove(&chunks[ci][cj], i | REF_ITEM)) {
		printf("WARNING: item %ld not removed from chunk %lu, %lu\n", i, ci, cj);
	}
}

void chunk_remove_fixture(long i) {
	struct Fixture c = fixtures[i];
	size_t ci = get_chunk(c.x);
	size_t cj = get_chunk(c.y);
	if (!chunk_remove(&chunks[ci][cj], i | REF_FIXTURE)) {
		printf("WARNING: fixture %ld not removed from chunk %lu, %lu\n", i, ci, cj);
	}
}

int high_water = 0;

void chunk_add_char(long i) {
	struct Char c = chars[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	struct Chunk *chunk = &chunks[ci][cj];
	if (chunk->total_num == CHUNK_BUFFER_SIZE) {
		printf("ERROR: chunk %lu, %lu is full\n", ci, cj);
		exit(1);
	}
	chunk->refs[chunk->total_num++] = i | REF_CHAR;
	high_water = max(high_water, chunk->total_num);
}
void chunk_add_item(long i) {
	struct Item c = items[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	struct Chunk *chunk = &chunks[ci][cj];
	if (chunk->total_num == CHUNK_BUFFER_SIZE) {
		printf("ERROR: chunk %lu, %lu is full\n", ci, cj);
		exit(1);
	}
	chunk->refs[chunk->total_num++] = i | REF_ITEM;
	high_water = max(high_water, chunk->total_num);
}
void chunk_add_fixture(long i) {
	struct Fixture c = fixtures[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	struct Chunk *chunk = &chunks[ci][cj];
	if (chunk->total_num == CHUNK_BUFFER_SIZE) {
		printf("ERROR: chunk %lu, %lu is full\n", ci, cj);
		exit(1);
	}
	chunk->refs[chunk->total_num++] = i | REF_FIXTURE;
	high_water = max(high_water, chunk->total_num);
}

void init() {
	item_num = 0;
	fixture_num = 0;
	char_num = 0;
	range (i, CHUNK_DIM) {
		range (j, CHUNK_DIM) {
			chunks[i][j].total_num = 0;
			range(k, CHUNK_BUFFER_SIZE) {
				chunks[i][j].refs[k] = 0;
			}
		}
	}
	range (i, 1000) {
		const int g = 1000; // granularity of randomness
		const num RANGE = DIM - 1;
		items[i].x = rand_int(g) * RANGE / g;
		items[i].y = rand_int(g) * RANGE / g;
		items[i].type = &item_types[i % item_type_count];
		items[i].change_frame = items[i].type->live_frames;
		items[i].held_by = -1;
		item_num++;
		chunk_add_item(i);
	}
	range (i, CHAR_INITIAL) {
		const int g = 1000; // granularity of randomness
		const num RANGE = DIM - 1;
		chars[i].x = rand_int(g) * RANGE / g;
		chars[i].y = rand_int(g) * RANGE / g;
		chars[i].held_item = -1;
		chars[i].input_count = 0;
		chars[i].goal = &recipes[i % recipe_count];
		char_num++;
		chunk_add_char(i);
	}
	printf("Spread %d characters, %d items, %d fixtures across %d chunks, highest was %d in one chunk\n", char_num, item_num, fixture_num, CHUNK_DIM*CHUNK_DIM, high_water);
}

ref find_nearest(num x, num y, num r, bool (*cond)(ref x)) {
	size_t cl = get_chunk(max(x - r, 1-DIM));
	size_t cr = get_chunk(min(x + r, DIM-1));
	size_t cu = get_chunk(max(y - r, 1-DIM));
	size_t cd = get_chunk(min(y + r, DIM-1));
	ref nearest = -1;
	long nearestqu = r * r;
	for (int di = cl; di <= cr; di++) {
		for (int dj = cu; dj <= cd; dj++) {
			struct Chunk *chunk = &chunks[di][dj];
			range(k, chunk->total_num) {
				ref r = chunk->refs[k];
				if (!cond(r)) continue;
				num itx, ity;
				if ((r & REF_SORT) == REF_CHAR) {
					itx = chars[r & REF_IND].x;
					ity = chars[r & REF_IND].y;
				} else if ((r & REF_SORT) == REF_ITEM) {
					itx = items[r & REF_IND].x;
					ity = items[r & REF_IND].y;
				} else if ((r & REF_SORT) == REF_FIXTURE) {
					itx = fixtures[r & REF_IND].x;
					ity = fixtures[r & REF_IND].y;
				} else {
					printf("Chunk contained unknown ref %x\n", r);
					exit(1);
				}
				num dx = itx - x;
				num dy = ity - y;
				num qu = dx*dx + dy*dy;
				if (qu < nearestqu) {
					nearest = r;
					nearestqu = qu;
				}
			}
		}
	}
	return nearest;
}

bool is_char(ref x) { return (x & REF_SORT) == REF_CHAR; }
bool is_item(ref x) { return (x & REF_SORT) == REF_ITEM; }
bool is_fixture(ref x) { return (x & REF_SORT) == REF_FIXTURE; }

ItemType target_item_type = NULL;
bool is_target_item_type(ref x) {
	return (x & REF_SORT) == REF_ITEM && items[x & REF_IND].type == target_item_type;
}

void simulate() {
	range (i, item_num) {
		if (items[i].type != NULL && 0 <= items[i].change_frame && items[i].change_frame <= frame)
		{
			items[i].type = items[i].type->turns_into;
			if (items[i].type != NULL) {
				items[i].change_frame += items[i].type->live_frames;
			}
		}
	}
	range (i, char_num) {
		num vx = 0, vy = 0;

		Recipe goal = chars[i].goal;
		if (goal == NULL) {
			continue;
		}
		if (goal->input_count == 0) {
			printf("Recipes without inputs currently not supported\n");
			exit(1);
		} else if (chars[i].held_item != -1) {
			bool dropit = false;
			if (chars[i].input_count == 0
				|| chars[i].input_count >= goal->input_count
				|| goal->inputs[chars[i].input_count] != items[chars[i].held_item].type
			) {
				dropit = true;
			} else {
				num dx = chars[i].craft_x - chars[i].x;
				num dy = chars[i].craft_y - chars[i].y;
				num qu = dx * dx + dy * dy;
				if (qu < REACH * REACH) {
					chars[i].inputs[chars[i].input_count] = chars[i].held_item;
					chars[i].input_count += 1;
					dropit = true;
					chars[i].craft_t = frame + goal->duration;
				} else {
					num scale = invsqrt_nr(qu / UNIT);
					vx = dx * scale / UNIT / 4;
					vy = dy * scale / UNIT / 4;
				}
			}
			if (dropit) {
				long it = chars[i].held_item;
				items[it].x = chars[i].x;
				items[it].y = chars[i].y;
				items[it].held_by = -1;
				chars[i].held_item = -1;
				chunk_add_item(it);
			}
		} else if (chars[i].input_count < goal->input_count) {
			target_item_type = goal->inputs[chars[i].input_count];
			ref target = find_nearest(chars[i].x, chars[i].y, AWARENESS, is_target_item_type);
			if (target == -1) {
				continue;
			}
			target &= REF_IND;
			num dx = items[target].x - chars[i].x;
			num dy = items[target].y - chars[i].y;
			num qu = dx * dx + dy * dy;
			if (qu < REACH * REACH || (chars[i].input_count == 0 && goal->input_count > 1)) {
				if (chars[i].input_count == 0) {
					chars[i].inputs[chars[i].input_count] = target;
					chars[i].input_count += 1;
					chars[i].craft_x = items[target].x;
					chars[i].craft_y = items[target].y;
					chars[i].craft_t = frame + goal->duration;
				} else {
					items[target].held_by = i;
					chars[i].held_item = target;
					chunk_remove_item(target);
				}
			} else {
				num scale = invsqrt_nr(qu / UNIT);
				vx = dx * scale / UNIT / 4;
				vy = dy * scale / UNIT / 4;
			}
		} else {
			bool stolen = false;
			range(inp, chars[i].input_count) {
				long j = chars[i].inputs[inp];
				if (items[j].type != goal->inputs[inp]) {
					stolen = true;
					chars[i].input_count = j;
					break;
				}
				num dx = items[j].x - chars[i].craft_x;
				num dy = items[j].y - chars[i].craft_y;
				num qu = dx * dx + dy * dy;
				if (qu > REACH * REACH) {
					stolen = true;
					chars[i].input_count = j;
					break;
				}
			}
			if (!stolen && frame >= chars[i].craft_t) {
				range(inp, chars[i].input_count) {
					long j = chars[i].inputs[inp];
					items[j].type = NULL;
					chunk_remove_item(j);
				}
				chars[i].input_count = 0;
				range(out, goal->output_count) {
					long j = 0;
					while (items[j].type != NULL) {
						j += 1;
						if (j >= ITEM_CAP) {
							printf("Hit item capacity\n");
							exit(1);
						}
					}
					items[j].type = goal->outputs[out];
					items[j].x = chars[i].craft_x;
					items[j].y = chars[i].craft_y;
					items[j].held_by = -1;
					int live_frames = goal->outputs[out]->live_frames;
					if (live_frames == -1) {
						items[j].change_frame = -1;
					} else {
						items[j].change_frame = frame + live_frames;
					}
					chunk_add_item(j);
				}
			}
		}

		{
			chunk_remove_char(i);
			chars[i].x += vx;
			chars[i].y += vy;
			if (chars[i].x >= DIM) {
				chars[i].x = DIM-1;
			} else if (chars[i].x <= -DIM) {
				chars[i].x = -DIM+1;
			}
			if (chars[i].y >= DIM) {
				chars[i].y = DIM-1;
			} else if (chars[i].y <= -DIM) {
				chars[i].y = -DIM+1;
			}
			chunk_add_char(i);
		}
	}
}

