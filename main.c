#include "graphics.h"

#include "time.h"

int frame = 0;
time_t start_time;

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
#define CHAR_INITIAL (IDIM * IDIM / 128)

#define STARTING_TEAM_SIZE 10

#define REACH (UNIT)
#define MAX_HEALTH 60

size_t char_num = 0;

enum CharStrategy {
	STRAT_DEAD,
	STRAT_STAKE,
	STRAT_FORM_LEAD,
	STRAT_FORM_FOLLOW,
	STRAT_HUNT,
};

struct Char {
	num x, y;
	enum CharStrategy strat;
	int health;
	long team;
	long leader;
	// long held_item;
} chars[CHAR_CAP];

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
	int scrap;
} fixtures[FIXTURE_CAP];

#define AWARENESS (UNIT_CTIME * 32)
#define SPREAD_CURVE (2*UNIT)
#define SPREAD_FORCE (100000*UNIT)
#define SPREAD_WALL (5000*UNIT)

#define CHUNK_SIZE AWARENESS
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
	range (i, 0) {
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
		chars[i].strat = STRAT_FORM_LEAD;
		chars[i].team = i;
		chars[i].leader = 1;
		chars[i].health = MAX_HEALTH;
		// chars[i].held_item = -1;
		char_num++;
		chunk_add_char(i);
	}
	printf("Spread %d characters, %d items, %d fixtures across %d chunks, highest was %d in one chunk\n", char_num, item_num, fixture_num, CHUNK_DIM*CHUNK_DIM, high_water);
}

void simulate() {
	range (i, char_num) {
		num vx = 0, vy = 0;
		const size_t ci = get_chunk(chars[i].x);
		const size_t cj = get_chunk(chars[i].y);

		const size_t cl = max(1, ci)-1;
		const size_t cr = min(CHUNK_DIM-1, ci+1);
		const size_t cu = max(1, cj)-1;
		const size_t cd = min(CHUNK_DIM-1, cj+1);
		if (chars[i].strat == STRAT_DEAD) {
			continue;
		}
		if (chars[i].strat == STRAT_STAKE) {
			for (int di = cl; di <= cr; di++) {
				for (int dj = cu; dj <= cd; dj++) {
					struct Chunk *chunk = &chunks[di][dj];
					range(k, chunk->total_num) {
						if ((chunk->refs[k] & REF_SORT) != REF_CHAR) continue;
						size_t j = chunk->refs[k] & REF_IND;
						if (i == j) continue;
						num dx = chars[i].x - chars[j].x;
						num dy = chars[i].y - chars[j].y;
						if ((dx*dx+dy*dy) < AWARENESS * AWARENESS) {
							// approximately exp(dx^2+dy^2) = 1/density
							// increasing SPREAD_CURVE changes the base from e
							// to something smaller => larger radius of effect
							num invdensity = (UNIT + dx*dx/SPREAD_CURVE)
								*(UNIT + dy*dy/SPREAD_CURVE);
							const num C = SPREAD_FORCE;
							vx += C*dx/invdensity;
							vy += C*dy/invdensity;
						}
					}
				}
			}
			{
				const num C = SPREAD_WALL;
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
				fixtures[f].scrap = 0;
				fixture_num++;
				chunk_add_fixture(f);
				chars[i].strat = STRAT_DEAD;
				// chars[i].factory = f;
			}
		} else if (chars[i].strat == STRAT_FORM_LEAD) {
			if (chars[i].leader >= STARTING_TEAM_SIZE) {
				chars[i].strat = STRAT_HUNT;
				chars[i].leader = -1;
			}
			long nearest = -1;
			long nearestqu = AWARENESS * AWARENESS;
			num nearestdx, nearestdy;
			for (int di = cl; di <= cr; di++) {
				for (int dj = cu; dj <= cd; dj++) {
					struct Chunk *chunk = &chunks[di][dj];
					range(k, chunk->total_num) {
						if ((chunk->refs[k] & REF_SORT) != REF_CHAR) continue;
						size_t j = chunk->refs[k] & REF_IND;
						if (chars[j].strat != STRAT_FORM_LEAD
								&& chars[j].strat != STRAT_HUNT) continue;
						if (chars[i].team == chars[j].team) continue;
						num dx = chars[j].x - chars[i].x;
						num dy = chars[j].y - chars[i].y;
						num qu = dx*dx + dy*dy;
						if (qu < nearestqu) {
							nearest = j;
							nearestqu = qu;
							nearestdx = dx;
							nearestdy = dy;
						}
					}
				}
			}
			if (nearest == -1) {
				num x = chars[i].x;
				num y = chars[i].y;
				num scale = invsqrt_nr((x*x+y*y)/UNIT);
				vx = -x*scale/UNIT/4;
				vy = -y*scale/UNIT/4;
			} else if (nearestqu < 64*REACH * REACH) {
				if (chars[nearest].strat == STRAT_HUNT) {
					chars[i].strat = STRAT_HUNT;
					chars[i].leader = -1;
					chars[i].team = chars[nearest].team;
				} else {
					long lead = nearest, follow = i;
					if (chars[i].leader > chars[nearest].leader) {
						lead = i, follow = nearest;
					}
					chars[lead].leader += chars[follow].leader;
					chars[follow].strat = STRAT_FORM_FOLLOW;
					chars[follow].leader = lead;
					chars[follow].team = chars[lead].team;
				}
			} else {
				num scale = invsqrt_nr(nearestqu/UNIT);
				vx = nearestdx * scale / UNIT / 4;
				vy = nearestdy * scale / UNIT / 4;
			}
		} else if (chars[i].strat == STRAT_FORM_FOLLOW) {
			long j = chars[i].leader;
			if (chars[j].strat == STRAT_FORM_LEAD) {
				num dx = chars[j].x - chars[i].x;
				num dy = chars[j].y - chars[i].y;
				num qu = dx*dx + dy*dy;
				if (qu > REACH * REACH) {
					num scale = invsqrt_nr(qu / UNIT);
					vx = dx * scale / UNIT / 4;
					vy = dy * scale / UNIT / 4;
				}
			} else {
				chars[i].strat = chars[j].strat;
				chars[i].leader = chars[j].leader;
				chars[i].team = chars[j].team;
			}
		} else if (chars[i].strat == STRAT_HUNT) {
			long nearest = -1;
			long nearestqu = AWARENESS * AWARENESS;
			num nearestdx, nearestdy;
			for (int di = cl; di <= cr; di++) {
				for (int dj = cu; dj <= cd; dj++) {
					struct Chunk *chunk = &chunks[di][dj];
					range(k, chunk->total_num) {
						if ((chunk->refs[k] & REF_SORT) != REF_CHAR) continue;
						size_t j = chunk->refs[k] & REF_IND;
						if (chars[j].team == chars[i].team) continue;
						if (chars[j].strat == STRAT_DEAD
								|| chars[j].health == 0) continue;
						num dx = chars[j].x - chars[i].x;
						num dy = chars[j].y - chars[i].y;
						num qu = dx*dx + dy*dy;
						if (qu < nearestqu) {
							nearest = j;
							nearestqu = qu;
							nearestdx = dx;
							nearestdy = dy;
						}
					}
				}
			}
			if (nearest == -1) {
				num x = chars[i].x;
				num y = chars[i].y;
				num scale = invsqrt_nr((x*x+y*y)/UNIT);
				vx = -x*scale/UNIT/4;
				vy = -y*scale/UNIT/4;
			} else if (nearestqu < 64*REACH * REACH) {
				chars[nearest].health--;
				if (chars[nearest].health <= 0) {
					chars[nearest].strat = STRAT_DEAD;
					chars[nearest].team = -1;
					chars[nearest].leader = -1;
				}
			} else {
				num scale = invsqrt_nr(nearestqu/UNIT);
				vx = nearestdx * scale / UNIT / 4;
				vy = nearestdy * scale / UNIT / 4;
			}
		} else {
			printf("WARNING unresolved strategy %d\n", chars[i].strat);
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
		if (items[i].type == ITEM_NULL || items[i].held_by != -1) continue;
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

		float col[3] = {0.5f, 0.5f, 0.5f};
		if (chars[i].team != -1) {
			bright((float)(chars[i].team % 12)*0.5f, col);
			if (chars[i].team / 12 % 2) {
				col[0] *= 0.5f;
				col[1] *= 0.5f;
				col[2] *= 0.5f;
			}
		}

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

