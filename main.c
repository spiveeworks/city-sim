#define FRAMERATE 60

#include "graphics.h"

#include "data.h"
#include "sim.h"

#include <time.h>

time_t start_time;

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
		if (items[i].type == NULL || items[i].held_by != -1) continue;
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

