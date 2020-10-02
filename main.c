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

void rect(struct Vertex* vertex_data, size_t *total,
	float xl, float yb, float xr, float yt, float r, float g, float b)
{
	struct Vertex vs[2][2] =
	{
		{
			{{xl, yb}, {r, g, b}, {0.0f, 0.0f}},
			{{xr, yb}, {r, g, b}, {0.0f, 0.0f}}
		},
		{
			{{xl, yt}, {r, g, b}, {0.0f, 0.0f}},
			{{xr, yt}, {r, g, b}, {0.0f, 0.0f}}
		}
	};
	vertex_data[(*total)++] = vs[0][0];
	vertex_data[(*total)++] = vs[1][0];
	vertex_data[(*total)++] = vs[0][1];
	vertex_data[(*total)++] = vs[1][0];
	vertex_data[(*total)++] = vs[1][1];
	vertex_data[(*total)++] = vs[0][1];
}

void line_by_num(
	struct Vertex* vertex_data, size_t *total,
	num x0_num, num y0_num, num x1_num, num y1_num,
	float thickness, float r, float g, float b
) {
	num dx = x1_num - x0_num;
	num dy = y1_num - y0_num;
	num qu = (dx*dx + dy*dy)/UNIT;
	num scale = invsqrt_nr(qu);
	num vx_num = dy*scale/UNIT;
	num vy_num = -dx*scale/UNIT;
	float vx = thickness * (float)vx_num / (float)DIM;
	float vy = -thickness * (float)vy_num / (float)DIM;
	float x0 = (float)x0_num / (float)DIM;
	float y0 = -(float)y0_num / (float)DIM;
	float x1 = (float)x1_num / (float)DIM;
	float y1 = -(float)y1_num / (float)DIM;
	struct Vertex vs[2][2] =
	{
		{
			{{x0-vx, y0-vy}, {r, g, b}, {0.0f, 0.0f}},
			{{x0+vx, y0+vy}, {r, g, b}, {0.0f, 0.0f}},
		},
		{
			{{x1-vx, y1-vy}, {r, g, b}, {0.0f, 0.0f}},
			{{x1+vx, y1+vy}, {r, g, b}, {0.0f, 0.0f}},
		},
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
	range (i, fixture_count) {
		Fixture fx = live_fixtures[i];
		if (fx->storage_count == 0) { continue; }
		float x = (float)fx->x / (float)DIM;
		float y = -(float)fx->y / (float)DIM;
		float dx = (float)fx->type->width/2 / (float)DIM;
		float dy = (float)fx->type->height/2 / (float)DIM;

		float col[3] = {0.5f, 0.5f, 0.5f};
		ItemType type = fx->storage[0].type;
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
		rect(vertex_data, &total, x-dx, y-dy, x+dx, y+dy, col[0], col[1], col[2]);
	}
	float dx = 0.95f/100.0f;
	range (i, char_count) {
		float x = (float)chars[i].x / (float)DIM;
		float y = -(float)chars[i].y / (float)DIM;

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
	range (i, obstacle_count) {
		float xl = (float)obstacles[i].l / (float)DIM;
		float xr = (float)obstacles[i].r / (float)DIM;
		float yb = -(float)obstacles[i].b / (float)DIM;
		float yt = -(float)obstacles[i].t / (float)DIM;
		rect(vertex_data, &total, xl, yb, xr, yt, 1.0F, 1.0F, 1.0F);
	}
	range (j, nav_node_count) {
		range (i, j) {
			if (nav_paths[(j*j-j)/2+i].next.i == j) {
			//if (nav_paths[(j*j-j)/2+i].next.i != ~0) {
				line_by_num(
					vertex_data, &total,
					nav_nodes[i].x, nav_nodes[i].y,
					nav_nodes[j].x, nav_nodes[j].y,
					0.2F,
					0.0F, 1.0F, 0.2F
				);
			}
		}
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

	initialize_nav_edges();

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

