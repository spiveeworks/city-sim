#include "graphics.h"

#include "time.h"

int frame = 0;

typedef int64_t num;
const num UNIT = 1ull << 16;

const num DIM = (1ull << 16) * 100;

#define CHAR_NUM 1000

struct Char {
	num x, y;
} chars[CHAR_NUM];

#define rand_int(g) ((rand() % (2*(g)+1))-(g))

void init() {
	range (i, CHAR_NUM) {
		const int g = 1000; // granularity of randomness
		const num RANGE = DIM * 7 / 8;
		chars[i].x = rand_int(g) * RANGE / g;
		chars[i].y = rand_int(g) * RANGE / g;
	}
}

void simulate() {
	for (size_t i = 1; i < CHAR_NUM; i++) {
		range (j, i-1) {
			num dx = chars[i].x - chars[j].x;
			num dy = chars[i].y - chars[j].y;
			num qu = dx*dx+dy*dy;
			if (qu != 0 && qu < UNIT*UNIT*100) {
				const num C = 10000*UNIT;
				chars[i].x += C*dx / qu;
				chars[i].y += C*dy / qu;
				chars[j].x -= C*dx / qu;
				chars[j].y -= C*dy / qu;
			}
		}
	}
	range (i, CHAR_NUM) {
		const num C = 5000*UNIT;
		chars[i].x += C/(chars[i].x-DIM);
		chars[i].x += C/(chars[i].x+DIM);
		chars[i].y += C/(chars[i].y-DIM);
		chars[i].y += C/(chars[i].y+DIM);
	}
	range (i, CHAR_NUM) {
		if (chars[i].x > DIM) {
			chars[i].x = DIM;
		} else if (chars[i].x < -DIM) {
			chars[i].x = -DIM;
		}
		if (chars[i].y > DIM) {
			chars[i].y = DIM;
		} else if (chars[i].y < -DIM) {
			chars[i].y = -DIM;
		}
	}
}

size_t build_vertex_data(struct Vertex* vertex_data) {
	size_t total = 0;
	float dx = 0.95f/100.0f;
	range (i, CHAR_NUM) {
		float x = (float)chars[i].x / (float)DIM;
		float y = (float)chars[i].y / (float)DIM;

		float c = (float)i*6.0f / (float)CHAR_NUM;
		float r, g, b;
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
	srand(time(NULL));
	struct GraphicsInstance gi = createGraphicsInstance();
	struct Graphics g = createGraphics(&gi);

	bool recreateGraphics = false;
	glfwSetWindowUserPointer(gi.window, &recreateGraphics);
	glfwSetFramebufferSizeCallback(gi.window, recordResize);

	init();

	while(!glfwWindowShouldClose(gi.window)) {
		glfwPollEvents();

		frame++;
		if (frame % 300 == 0) {
			printf("reached frame %d (%d seconds)\n", frame, frame/60);
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

