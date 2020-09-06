#include "graphics.h"

#include "time.h"

int frame = 0;
time_t start_time;
#define FRAMERATE 60

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

#define NAME_BUF_CAP 65536
char name_buf[NAME_BUF_CAP];
size_t name_buf_len = 0;

char *alloc_name(const char *in, size_t len) {
	char *out = &name_buf[name_buf_len];
	name_buf_len += len + 1;
	if (name_buf_len > NAME_BUF_CAP) {
		printf("Ran out of room in name buffer\n");
		exit(1);
	}
	memcpy(out, in, len);
	out[len] = '\0';
	return out;
}

#define ITEM_TYPE_CAP 256
struct ItemType {
	char *name;
	struct ItemType *turns_into;
	int live_frames;
	uint8_t color[3];
	bool color_initialized;
} item_types[ITEM_TYPE_CAP];
size_t item_type_count = 0;
typedef struct ItemType *ItemType;

#define RECIPE_CAP 256
#define RECIPE_INPUT_CAP 8
struct Recipe {
	char *name;
	int duration;
	uint8_t input_count;
	uint8_t output_count;
	ItemType inputs[RECIPE_INPUT_CAP];
	ItemType outputs[RECIPE_INPUT_CAP];
} recipes[RECIPE_CAP];
size_t recipe_count = 0;
typedef struct Recipe *Recipe;

enum TokenType {
	TOKEN_NONE,
	TOKEN_INVALID,
	TOKEN_ALPHA,
	TOKEN_NUM,
	TOKEN_SYMBOL, // unused, but no token greater than this has any alphanum
	TOKEN_EQ,
};

struct Token {
	enum TokenType type;
	char *start;
	char *end;
	// only initialized if type is TOKEN_NUM
	bool negate;
	unsigned whole;
	unsigned nume;
	unsigned denom;
};

void token_get(struct Token *out, char *str) {
	out->start = str;
	out->end = str;
	out->type = TOKEN_NONE;
	while (true) {
		char c = *out->end;
		if (c == '\0' || c == '\n') {
			break;
		} else if (c == ' ' || c == '\t') {
			if (out->type != TOKEN_NONE) {
				break;
			}
			out->start += 1;
		} else if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')) {
			if (out->type == TOKEN_NONE) {
				out->type = TOKEN_ALPHA;
			} else if (out->type == TOKEN_NUM) {
				out->type = TOKEN_INVALID;
			} else if (out->type >= TOKEN_SYMBOL) {
				break;
			}
		} else if ('0' <= c && c <= '9') {
			if (out->type == TOKEN_NONE) {
				out->type = TOKEN_NUM;
				out->negate = false;
				out->whole = c - '0';
				out->nume = 0;
				out->denom = 1;
			} else if (out->type == TOKEN_NUM) {
				out->whole = out->whole * 10 + c - '0';
			} else if (out->type >= TOKEN_SYMBOL) {
				break;
			}
		} else if (c == '=') {
			if (out->type == TOKEN_NONE) {
				out->type = TOKEN_EQ;
			} else {
				break;
			}
		}
		out->end += 1;
	}
	size_t tok_len = out->end - out->start;
}

enum TokenType token_get_type(char **str) {
	struct Token t;
	token_get(&t, *str);
	*str = t.end;
	return t.type;
}

#define token_cmp(t, str) (strncmp((t).start, str, (t).end - (t).start) == 0)

ItemType token_get_item_type(char **str) {
	struct Token t;
	token_get(&t, *str);
	*str = t.end;
	if (t.type != TOKEN_ALPHA) {
		printf("Expected item type\n");
		exit(1);
	}
	range(i, item_type_count) {
		if (token_cmp(t, item_types[i].name)) {
			return &item_types[i];
		}
	}
	printf("Unknown item type \"%s\"\n", alloc_name(t.start, t.end - t.start));
	exit(1);
}

num token_get_num(char **str, num unit) {
	struct Token t;
	token_get(&t, *str);
	*str = t.end;
	if (t.type != TOKEN_NUM) {
		printf("Expected number, got \"%s\"\n", alloc_name(t.start, t.end - t.start));
		exit(1);
	}
	return t.whole * unit + t.nume * unit / t.denom;
}

void parse_data() {
	FILE* f = fopen("data.txt", "r");

	enum {
		PARSE_STATE_NULL,
		PARSE_STATE_ITEM,
		PARSE_STATE_RECIPE,
	} focus = PARSE_STATE_NULL;
	item_type_count = 0;
	// recipe_count = 0;

	while (feof(f) == 0) {
		const int STR_CAP = 256;
		char str[STR_CAP];
		fgets(str, STR_CAP, f);
		char *curr = str;
		struct Token t;
		token_get(&t, curr);
		curr = t.end;
		if (t.type == TOKEN_NONE) {
			continue;
		}
		if (t.type != TOKEN_ALPHA) {
			printf("Expected keyword at start of line\n");
			exit(1);
		}
		if (token_cmp(t, "item")) {
			if (item_type_count >= ITEM_TYPE_CAP) {
				printf("Hit item type capacity\n");
				exit(1);
			}
			ItemType out = &item_types[item_type_count];
			item_type_count += 1;
			focus = PARSE_STATE_ITEM;
			token_get(&t, curr);
			curr = t.end;
			if (t.type != TOKEN_ALPHA) {
				printf("Expected name after keyword \"item\"\n");
				exit(1);
			}
			out->name = alloc_name(t.start, t.end - t.start);
			out->turns_into = NULL;
			out->live_frames = -1;
			out->color_initialized = false;
			if (token_get_type(&curr) != TOKEN_NONE) {
				printf("Expected end of line after item declaration\n");
				exit(1);
			}
		} else if (token_cmp(t, "transform")) {
			if (focus != PARSE_STATE_ITEM || item_type_count == 0) {
				printf("Only items can have transformation rules\n");
				exit(1);
			}
			token_get(&t, curr);
			curr = t.end;
			bool into;
			if (token_cmp(t, "from")) {
				into = false;
			} else if (token_cmp(t, "into")) {
				into = true;
			} else {
				printf("Expected \"from\" or \"into\", got \"%s\"\n", alloc_name(t.start, t.end - t.start));
				exit(1);
			}
			ItemType obj = token_get_item_type(&curr);
			token_get(&t, curr);
			curr = t.end;
			if (!token_cmp(t, "after")) {
				printf("Expected \"after\" got \"%s\"\n", alloc_name(t.start, t.end - t.start));
				exit(1);
			}
			int live_frames = (int)token_get_num(&curr, FRAMERATE);
			ItemType from_type;
			ItemType into_type;
			if (into) {
				from_type = &item_types[item_type_count - 1];
				into_type = obj;
			} else {
				into_type = &item_types[item_type_count - 1];
				from_type = obj;
			}
			from_type->turns_into = into_type;
			from_type->live_frames = live_frames;
		} else if (token_cmp(t, "recipe")) {
			if (recipe_count >= RECIPE_CAP) {
				printf("Hit recipe capacity\n");
				exit(1);
			}
			Recipe out = &recipes[recipe_count];
			recipe_count += 1;
			focus = PARSE_STATE_RECIPE;
			token_get(&t, curr);
			curr = t.end;
			if (t.type != TOKEN_ALPHA) {
				printf("Expected name after keyword \"recipe\"\n");
				exit(1);
			}
			out->name = alloc_name(t.start, t.end - t.start);
			out->duration = 0;
			out->input_count = 0;
			out->output_count = 0;
		} else if (token_cmp(t, "duration")) {
			if (focus != PARSE_STATE_RECIPE || recipe_count == 0) {
				printf("Only recipes can have durations\n");
				exit(1);
			}
			recipes[recipe_count - 1].duration = token_get_num(&curr, FRAMERATE);
		} else if (token_cmp(t, "input")) {
			if (focus != PARSE_STATE_RECIPE || recipe_count == 0) {
				printf("Only recipes can have inputs\n");
				exit(1);
			}
			Recipe out = &recipes[recipe_count - 1];
			if (out->input_count == RECIPE_INPUT_CAP) {
				printf("Hit input capacity (%d)\n", RECIPE_INPUT_CAP);
				exit(1);
			}
			out->inputs[out->input_count] = token_get_item_type(&curr);
			out->input_count += 1;
		} else if (token_cmp(t, "output")) {
			if (focus != PARSE_STATE_RECIPE || recipe_count == 0) {
				printf("Only recipes can have outputs\n");
				exit(1);
			}
			Recipe out = &recipes[recipe_count - 1];
			if (out->output_count == RECIPE_INPUT_CAP) {
				printf("Hit output capacity (%d)\n", RECIPE_INPUT_CAP);
				exit(1);
			}
			out->outputs[out->output_count] = token_get_item_type(&curr);
			out->output_count += 1;
		} else {
			printf("Unknown keyword %s\n", alloc_name(t.start, t.end - t.start));
			exit(1);
		}
		token_get(&t, curr);
		if (t.type != TOKEN_NONE) {
			size_t len = strlen(t.start);
			while (t.start[len-1] == '\r' || t.start[len-1] == '\n') {
				len -= 1;
			}
			printf("Expected end of line, got \"%s\"\n", alloc_name(t.start, strlen(t.start)));
			exit(1);
		}
	}

	fclose(f);
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
	// enum CharTask task;
	// long held_item;
} chars[CHAR_CAP];

#define ITEM_CAP (IDIM * IDIM / 16)

size_t item_num = 0;

struct Item {
	num x, y;
	// long held_by;
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
		// items[i].held_by = -1;
		item_num++;
		chunk_add_item(i);
	}
	range (i, CHAR_INITIAL) {
		const int g = 1000; // granularity of randomness
		const num RANGE = DIM * 7 / 8;
		chars[i].x = rand_int(g) * RANGE / g;
		chars[i].y = rand_int(g) * RANGE / g;
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

size_t selected_char = ~0;

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
		if (items[i].type == NULL /*|| items[i].held_by != -1*/) continue;
		float x = (float)items[i].x / (float)DIM;
		float y = (float)items[i].y / (float)DIM;

		float col[3] = {0.5f, 0.5f, 0.5f};
		ItemType type = items[i].type;
		if (type->color_initialized) {
			col[0] = (float)type->color[0]/255.0f;
			col[1] = (float)type->color[1]/255.0f;
			col[2] = (float)type->color[2]/255.0f;
		} else {
			bright((float)((type - item_types) % 12)*0.5f, col);
			if ((type - item_types) / 12 % 2) {
				col[0] *= 0.5f;
				col[1] *= 0.5f;
				col[2] *= 0.5f;
			}
		}
		square(vertex_data, &total, x, y, dx, col[0], col[1], col[2]);
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
		bright((float)(i % 12)*0.5f, col);
		if (i / 12 % 2) {
			col[0] *= 0.5f;
			col[1] *= 0.5f;
			col[2] *= 0.5f;
		}
		if (i == selected_char) {
			col[0] = 1.0f;
			col[1] = 1.0f;
			col[2] = 1.0f;
		}

		circle(vertex_data, &total, x, y, dx, col[0], col[1], col[2]);
	}
	return total;
}

void recordResize(GLFWwindow *window, int width, int height) {
	bool *recreateGraphics = glfwGetWindowUserPointer(window);
	*recreateGraphics = true;
}

const num MOUSE_RANGE = DIM_CTIME / 32;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	double glfw_x;
	double glfw_y;
	glfwGetCursorPos(window, &glfw_x, &glfw_y);
	int width;
	int height;
	glfwGetFramebufferSize(window, &width, &height);
	num x = 2 * DIM * (num)glfw_x / width - DIM;
	num y = 2 * DIM * (num)glfw_y / height - DIM;
	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
		ref nearest = find_nearest(x, y, MOUSE_RANGE, is_char);
		if (nearest != -1) {
			selected_char = nearest & REF_IND;
		} else {
			selected_char = -1;
		}
	} else if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_RIGHT) {
	}
}

int main() {
	srand(time(&start_time));

	parse_data();
	printf("Total of %lu item types\n", item_type_count);

	struct GraphicsInstance gi = createGraphicsInstance();
	struct Graphics g = createGraphics(&gi);

	bool recreateGraphics = false;
	glfwSetWindowUserPointer(gi.window, &recreateGraphics);
	glfwSetFramebufferSizeCallback(gi.window, recordResize);
	glfwSetMouseButtonCallback(gi.window, mouse_button_callback);

	init();

	while(!glfwWindowShouldClose(gi.window)) {
		glfwPollEvents();

		frame++;
		if (frame % 600 == 0) {
			printf("reached frame %d (%d seconds)\n", frame, time(NULL)-start_time);
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

