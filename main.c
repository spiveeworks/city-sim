#include "graphics.h"

#include "time.h"

int frame = 0;
time_t start_time;

typedef int64_t num;
#define UNIT_CTIME (1ll << 16)
const num UNIT = UNIT_CTIME;

#define IDIM 128
#define DIM_CTIME (UNIT_CTIME * IDIM)
const num DIM = DIM_CTIME;

#define CHAR_NUM (IDIM * IDIM / 4)

struct Char {
	num x, y;
	int still;
} chars[CHAR_NUM];

int random_walking = 0;
int stationary = 0;

struct CharRef {
	long i;
};

#define AWARENESS (UNIT_CTIME * 8)
#define WIGGLE_SCALE (1000*UNIT_CTIME)
#define DISPERSION_SCALE (50000ll*UNIT_CTIME)
#define DISPERSION_BOUNDARY (10000ll*UNIT_CTIME)
#define DISPERSION_DEADZONE (UNIT_CTIME/50)

#define CHUNK_SIZE AWARENESS
#define CHUNK_DIM (2 * DIM_CTIME / CHUNK_SIZE + 1)
#define CHUNK_DEPTH 20
struct Chunk {
	struct CharRef chars[CHUNK_DEPTH];
} chunks[CHUNK_DIM][CHUNK_DIM];

#define get_chunk(x) (((x)+DIM)/CHUNK_SIZE)

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
	printf("ERROR: chunk %lu, %lu is full\n", ci, cj);
	exit(1);
}

void init() {
	range (i, CHUNK_DIM) {
		range (j, CHUNK_DIM) {
			range(k, CHUNK_DEPTH) {
				chunks[i][j].chars[k].i = -1;
			}
		}
	}
	range (i, CHAR_NUM) {
		const int g = 1000; // granularity of randomness
		const num RANGE = DIM * 7 / 8;
		chars[i].x = rand_int(g) * RANGE / g;
		chars[i].y = rand_int(g) * RANGE / g;
		chars[i].still = 0;
		chunk_add_char(i);
	}
	printf("Spread %d characters across %d chunks, highest was %d in one chunk\n", CHAR_NUM, CHUNK_DIM*CHUNK_DIM, high_water);
}

void simulate() {
	for (size_t i = stationary; i < CHAR_NUM; i++) {
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
						num C = DISPERSION_SCALE;
						num invdensity = (UNIT + dx*dx/UNIT)*(UNIT + dy*dy/UNIT);
						vx += C*dx/invdensity;
						vy += C*dy/invdensity;
					}
				}
			}
		}
		if (CHAR_NUM-i <= random_walking) {
			vx = rand_int(10)*WIGGLE_SCALE/10*chars[i].still/frame;
			vy = rand_int(10)*WIGGLE_SCALE/10*chars[i].still/frame;
		}
		{
			vx += DISPERSION_BOUNDARY/(chars[i].x-DIM);
			vx += DISPERSION_BOUNDARY/(chars[i].x+DIM);
			vy += DISPERSION_BOUNDARY/(chars[i].y-DIM);
			vy += DISPERSION_BOUNDARY/(chars[i].y+DIM);
		}

		vx = vx * (10 + frame - chars[i].still) / frame;
		vy = vy * (10 + frame - chars[i].still) / frame;
		num vqu = vx*vx+vy*vy;
		num dz = DISPERSION_DEADZONE;
		if (vqu > dz * dz) {
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
		} else {
			chars[i].still++;
		}
	}
}

size_t build_vertex_data(struct Vertex* vertex_data) {
	size_t total = 0;
	float dx = 0.95f/100.0f;
	range (i, CHAR_NUM) {
		float x = (float)chars[i].x / (float)DIM;
		float y = (float)chars[i].y / (float)DIM;

		float c;
		float r, g, b;
		if (i < stationary) {
			c = 5.0f;
		} else if (CHAR_NUM-i <= random_walking) {
			c = 0.0f;
		} else {
			c = (float)chars[i].still/(float)frame * 5.5f;
		}
		if (c < 1.0f) {
			r = 1.0f, g = c, b = 0.0f;
		} else if (c < 2.0f) {
			r = 2.0f-c, g = 1.0f, b = 0.0f;
		} else if (c < 3.0f) {
			r = 0.0f, g = 1.0f, b = c-2.0f;
		} else if (c < 4.0f) {
			r = 0.0f, g = 4.0f-c, b = 1.0f;
		} else if (c < 5.0f) {
			r = c-4.0f, g = 0.0f, b = 1.0f;
		} else {
			r = 1.0f, g = 0.0f, b = 6.0f-c;
		}

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
		vertex_data[total++] = vs[0][0];
		vertex_data[total++] = vs[1][0];
		vertex_data[total++] = vs[0][1];
		vertex_data[total++] = vs[1][0];
		vertex_data[total++] = vs[1][1];
		vertex_data[total++] = vs[0][1];
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
		//stationary = frame % (CHAR_NUM + 60);
		if (1+stationary == 0) {
			printf("reached frame %d (%d seconds)\n", frame, time(NULL)-start_time);
			init();
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

