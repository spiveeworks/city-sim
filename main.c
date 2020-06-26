#include "graphics.h"

#include "time.h"

int frame = 0;
time_t start_time;

typedef int64_t num;
#define UNIT_CTIME (1ull << 16)
const num UNIT = UNIT_CTIME;

#define IDIM 128
#define DIM_CTIME (UNIT_CTIME * IDIM)
const num DIM = DIM_CTIME;

enum ItemType {
	ITEM_NULL,
	ITEM_SCRAP,
};

#define ITEM_CAP (IDIM * IDIM / 16)

size_t item_num = 0;

struct Item {
	num x, y;
	long held_by;
	enum ItemType type;
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
} fixtures[FIXTURE_CAP];

#define CHAR_CAP (IDIM * IDIM / 4)
#define CHAR_INITIAL (IDIM * IDIM / 128)

size_t char_num = 0;

enum CharStrategy {
	STRAT_NULL,
	STRAT_STAKE,
	STRAT_GATHER,
};

struct Char {
	num x, y;
	enum CharStrategy strat;
	long factory;
} chars[CHAR_CAP];

// inline these things
struct ItemRef {
	long i;
};

struct FixtureRef {
	long i;
};

struct CharRef {
	long i;
};

#define AWARENESS (UNIT_CTIME * 32)

#define CHUNK_SIZE AWARENESS
#define CHUNK_DIM (2 * DIM_CTIME / CHUNK_SIZE + 1)
#define CHUNK_DEPTH 50
// @Robustness combined array for these reference things?
struct Chunk {
	struct ItemRef items[CHUNK_DEPTH];
	struct FixtureRef fixtures[CHUNK_DEPTH];
	struct CharRef chars[CHUNK_DEPTH];
} chunks[CHUNK_DIM][CHUNK_DIM];

#define get_chunk(x) (((x)+DIM)/CHUNK_SIZE)

void chunk_remove_item(long i) {
	struct Item c = items[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	range (k, CHUNK_DEPTH) {
		if (chunks[ci][cj].items[k].i == i) {
			chunks[ci][cj].items[k].i = -1;
			return;
		}
	}
	printf("WARNING: item %ld not removed from chunk %lu, %lu\n", i, ci, cj);
}

void chunk_remove_fixture(long i) {
	struct Fixture c = fixtures[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	range (k, CHUNK_DEPTH) {
		if (chunks[ci][cj].fixtures[k].i == i) {
			chunks[ci][cj].fixtures[k].i = -1;
			return;
		}
	}
	printf("WARNING: fixture %ld not removed from chunk %lu, %lu\n", i, ci, cj);
}

void chunk_remove_char(long i) {
	struct Char c = chars[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	range (k, CHUNK_DEPTH) {
		if (chunks[ci][cj].chars[k].i == i) {
			chunks[ci][cj].chars[k].i = -1;
			return;
		}
	}
	printf("WARNING: char %ld not removed from chunk %lu, %lu\n", i, ci, cj);
}

int high_water = 0;

void chunk_add_item(long i) {
	struct Item c = items[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	range (k, CHUNK_DEPTH) {
		if (chunks[ci][cj].items[k].i == -1) {
			chunks[ci][cj].items[k].i = i;
			if (k > high_water) {
				high_water = k;
			}
			return;
		}
	}
	printf("ERROR: chunk %lu, %lu is full of items\n", ci, cj);
	exit(1);
}
void chunk_add_fixture(long i) {
	struct Fixture c = fixtures[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	range (k, CHUNK_DEPTH) {
		if (chunks[ci][cj].fixtures[k].i == -1) {
			chunks[ci][cj].fixtures[k].i = i;
			if (k > high_water) {
				high_water = k;
			}
			return;
		}
	}
	printf("ERROR: chunk %lu, %lu is full of fixtures\n", ci, cj);
	exit(1);
}
void chunk_add_char(long i) {
	struct Char c = chars[i];
	size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
	range (k, CHUNK_DEPTH) {
		if (chunks[ci][cj].chars[k].i == -1) {
			chunks[ci][cj].chars[k].i = i;
			if (k > high_water) {
				high_water = k;
			}
			return;
		}
	}
	printf("ERROR: chunk %lu, %lu is full of chars\n", ci, cj);
	exit(1);
}

void init() {
	item_num = 0;
	fixture_num = 0;
	char_num = 0;
	range (i, CHUNK_DIM) {
		range (j, CHUNK_DIM) {
			range(k, CHUNK_DEPTH) {
				chunks[i][j].items[k].i = -1;
				chunks[i][j].fixtures[k].i = -1;
				chunks[i][j].chars[k].i = -1;
			}
		}
	}
	range (i, ITEM_CAP) {
		const int g = 1000; // granularity of randomness
		const num RANGE = DIM - 1;
		items[i].x = rand_int(g) * RANGE / g;
		items[i].y = rand_int(g) * RANGE / g;
		items[i].type = ITEM_SCRAP;
		items[i].held_by = -1;
		item_num++;
		chunk_add_item(i);
	}
	range (i, CHAR_INITIAL) {
		const int g = 1000; // granularity of randomness
		const num RANGE = DIM * 7 / 8;
		chars[i].x = rand_int(g) * RANGE / g;
		chars[i].y = rand_int(g) * RANGE / g;
		chars[i].strat = STRAT_STAKE;
		chars[i].factory = -1;
		char_num++;
		chunk_add_char(i);
	}
	printf("Spread %d characters, %d items, %d fixtures across %d chunks, highest was %d in one chunk\n", char_num, item_num, fixture_num, CHUNK_DIM*CHUNK_DIM, high_water);
}

void simulate() {
	range (i, char_num) {
		/* consider the density function:
		 * exp(-dx^2-dy^2)
		 *   where dx = chars[i].x - chars[j].x
		 *         dy = chars[i].y - chars[j].y
		 *
		 * This is the chance of char[j] getting near char[i] by random walk
		 * after some amount of time, and by summing over all j we can
		 * represent the chance of any other character reaching char[i] after
		 * that amount of time.
		 *
		 * The direction of increasing density is given by calculus:
		 * dir_x = -2*dx*exp(-dx^2-dy^2)
		 * dir_y = -2*dy*exp(-dx^2-dy^2)
		 * negating again gives the direction of decreasing density, by moving
		 * away from char[j]
		 *
		 * now we can sum over these directions to get the direction in which
		 * density decreases overall, which is exactly what we want
		 *
		 * also we can approximate the exp function as follows:
		 *    exp(-dx^2-dy^2)
		 *  = 1/exp(dx^2+dy^2)
		 *  = 1/(exp(dx^2)exp(dy^2))
		 * ~= 1/((1+dx^2)(1+dy^2))
		 */
		num vx = 0, vy = 0;
		const size_t ci = get_chunk(chars[i].x);
		const size_t cj = get_chunk(chars[i].y);

		const size_t cl = max(1, ci)-1;
		const size_t cr = min(CHUNK_DIM-1, ci+1);
		const size_t cu = max(1, cj)-1;
		const size_t cd = min(CHUNK_DIM-1, cj+1);
		if (chars[i].strat == STRAT_STAKE) {
			for (int di = cl; di <= cr; di++) {
				for (int dj = cu; dj <= cd; dj++) {
					struct Chunk *chunk = &chunks[di][dj];
					range(k, CHUNK_DEPTH) {
						if (chunk->chars[k].i == -1) continue;
						size_t j = chunk->chars[k].i;
						if (i == j) continue;
						num dx = chars[i].x - chars[j].x;
						num dy = chars[i].y - chars[j].y;
						if ((dx*dx+dy*dy) < AWARENESS * AWARENESS) {
							// approximately exp(dx^2+dy^2) = 1/density
							num invdensity = (UNIT + dx*dx/UNIT)*(UNIT + dy*dy/UNIT);
							const num C = 50000*UNIT;
							vx += C*dx/invdensity;
							vy += C*dy/invdensity;
						}
					}
				}
			}
			{
				const num C = 10000*UNIT;
				vx += C/(chars[i].x-DIM);
				vx += C/(chars[i].x+DIM);
				vy += C/(chars[i].y-DIM);
				vy += C/(chars[i].y+DIM);
			}
			if (vx * vx + vy * vy < UNIT/100) {
				long f = fixture_num;
				fixtures[f].x = chars[i].x;
				fixtures[f].y = chars[i].y;
				fixtures[f].type = FIXTURE_FACTORY;
				fixture_num++;
				chunk_add_fixture(f);
				chars[i].strat = STRAT_GATHER;
				chars[i].factory = f;
			}
		} else if (chars[i].strat == STRAT_GATHER) {
			for (int di = cl; di <= cr; di++) {
				for (int dj = cu; dj <= cd; dj++) {
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

void bright(float c, float *col) {
	if (c < 1.0f) {
		col[0] = 1.0f, col[1] = c, col[2] = 0.0f;
	} else if (c < 2.0f) {
		col[0] = 2.0f-c, col[1] = 1.0f, col[2] = 0.0f;
	} else if (c < 3.0f) {
		col[0] = 0.0f, col[1] = 1.0f, col[2] = c-2.0f;
	} else if (c < 4.0f) {
		col[0] = 0.0f, col[1] = 4.0f-c, col[2] = 1.0f;
	} else if (c < 5.0f) {
		col[0] = c-4.0f, col[1] = 0.0f, col[2] = 1.0f;
	} else {
		col[0] = 1.0f, col[1] = 0.0f, col[2] = 6.0f-c;
	}
}

void circle(struct Vertex* vertex_data, size_t *total,
	float x, float y, float dx, float r, float g, float b)
{
	struct Vertex vs[2][2] =
	{
		{
			{{x-dx, y-dx}, {r, g, b}, {-1.0f, -1.0f}},
			{{x+dx, y-dx}, {r, g, b}, { 1.0f, -1.0f}},
		},
		{
			{{x-dx, y+dx}, {r, g, b}, {-1.0f,  1.0f}},
			{{x+dx, y+dx}, {r, g, b}, { 1.0f,  1.0f}},
		}
	};
	vertex_data[(*total)++] = vs[0][0];
	vertex_data[(*total)++] = vs[1][0];
	vertex_data[(*total)++] = vs[0][1];
	vertex_data[(*total)++] = vs[1][0];
	vertex_data[(*total)++] = vs[1][1];
	vertex_data[(*total)++] = vs[0][1];
}

void square(struct Vertex* vertex_data, size_t *total,
	float x, float y, float dx, float r, float g, float b)
{
	struct Vertex vs[2][2] =
	{
		{
			{{x-dx, y-dx}, {r, g, b}, {0.0f, 0.0f}},
			{{x+dx, y-dx}, {r, g, b}, {0.0f, 0.0f}}
		},
		{
			{{x-dx, y+dx}, {r, g, b}, {0.0f, 0.0f}},
			{{x+dx, y+dx}, {r, g, b}, {0.0f, 0.0f}}
		}
	};
	vertex_data[(*total)++] = vs[0][0];
	vertex_data[(*total)++] = vs[1][0];
	vertex_data[(*total)++] = vs[0][1];
	vertex_data[(*total)++] = vs[1][0];
	vertex_data[(*total)++] = vs[1][1];
	vertex_data[(*total)++] = vs[0][1];
}

size_t build_vertex_data(struct Vertex* vertex_data) {
	size_t total = 0;
	float dx = 0.95f/100.0f;
	range (i, item_num) {
		float x = (float)items[i].x / (float)DIM;
		float y = (float)items[i].y / (float)DIM;

		square(vertex_data, &total, x, y, dx, 0.0f, 0.0f, 0.7f);
	}
	range (i, fixture_num) {
		float x = (float)fixtures[i].x / (float)DIM;
		float y = (float)fixtures[i].y / (float)DIM;

		square(vertex_data, &total, x, y, dx, 0.5f, 0.5f, 0.5f);
	}
	range (i, char_num) {
		float x = (float)chars[i].x / (float)DIM;
		float y = (float)chars[i].y / (float)DIM;

		float col[3];
		bright(chars[i].strat-1, col);

		circle(vertex_data, &total, x, y, dx, col[0], col[1], col[2]);
	}
	return total;
}

void recordResize(GLFWwindow *window, int width, int height) {
	bool *recreateGraphics = glfwGetWindowUserPointer(window);
	*recreateGraphics = true;
}

int main() {
	srand(time(&start_time));
	struct GraphicsInstance gi = createGraphicsInstance();
	struct Graphics g = createGraphics(&gi);

	bool recreateGraphics = false;
	glfwSetWindowUserPointer(gi.window, &recreateGraphics);
	glfwSetFramebufferSizeCallback(gi.window, recordResize);

	init();

	while(!glfwWindowShouldClose(gi.window)) {
		glfwPollEvents();

		frame++;
		if (frame % 600 == 0) {
			printf("reached frame %d (%d seconds)\n", frame, time(NULL)-start_time);
			// init();
		}

		simulate();

		if (recreateGraphics || !drawFrame(&gi, &g)) {
			int width;
			int height;
			glfwGetFramebufferSize(gi.window, &width, &height);
			while (width == 0 && height == 0) {
				glfwGetFramebufferSize(gi.window, &width, &height);
				glfwWaitEvents();
			}

			destroyGraphics(&gi, &g);
			g = createGraphics(&gi);
			recreateGraphics = false;
		}
	}

	destroyGraphics(&gi, &g);
	destroyGraphicsInstance(&gi);

	return 0;
}

