#include "graphics.h"

#include "time.h"

int frame = 0;

typedef int64_t num;
const num UNIT = 1ull << 16;

const num DIM = (1ull << 16) * 100;

#define CHAR_NUM 1000

struct Char {
	num x, y;
	num velx, vely;
} chars[CHAR_NUM];

void simulate() {
	const num MAX_VEL = UNIT *10/60;
	const num MAX_ACC = MAX_VEL / 10;
	range (i, CHAR_NUM) {
		chars[i].x += chars[i].velx;
		chars[i].y += chars[i].vely;
		const int g = 8; // granularity of randomness
		chars[i].velx += ((rand() % (2*g+1)) - g)*MAX_ACC/g;
		chars[i].vely += ((rand() % (2*g+1)) - g)*MAX_ACC/g;
	}
	range (i, CHAR_NUM) {
		if (chars[i].x > DIM) {
			chars[i].x = DIM;
			chars[i].velx = 0;
		} else if (chars[i].x < -DIM) {
			chars[i].x = -DIM;
			chars[i].velx = 0;
		}
		if (chars[i].y > DIM) {
			chars[i].y = DIM;
			chars[i].vely = 0;
		} else if (chars[i].y < -DIM) {
			chars[i].y = -DIM;
			chars[i].vely = 0;
		}
		if (chars[i].velx > MAX_VEL) {
			chars[i].velx = MAX_VEL;
		} else if (chars[i].velx < -MAX_VEL) {
			chars[i].velx = -MAX_VEL;
		}
		if (chars[i].vely > MAX_VEL) {
			chars[i].vely = MAX_VEL;
		} else if (chars[i].vely < -MAX_VEL) {
			chars[i].vely = -MAX_VEL;
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

