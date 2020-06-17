#include "graphics.h"

int frame = 0;

size_t build_vertex_data(struct Vertex* vertex_data) {
	size_t total = 0;
	float dx = 0.95f/100.0f;
	range (i, 100+1) {
		float x = -1.0f + (float)i / 50.0f;
		float r = (float)i / 100.0f;
		range(j, 100+1) {
			float y = -1.0f + (float)j / 50.0f;
			float g = (float)j / 100.0f;
			struct Vertex vs[2][2] =
			{
				{
					{{x-dx, y-dx}, {r, g, 1.0f-g}, {-1.0f, -1.0f}},
					{{x+dx, y-dx}, {r, g, 1.0f-g}, { 1.0f, -1.0f}},
				},
				{
					{{x-dx, y+dx}, {r, g, 1.0f-g}, {-1.0f,  1.0f}},
					{{x+dx, y+dx}, {r, g, 1.0f-g}, { 1.0f,  1.0f}},
				}
			};
			vertex_data[total++] = vs[0][0];
			vertex_data[total++] = vs[1][0];
			vertex_data[total++] = vs[0][1];
			vertex_data[total++] = vs[1][0];
			vertex_data[total++] = vs[1][1];
			vertex_data[total++] = vs[0][1];
		}
	}
	if (frame * 60 < total) {
		total = 60*frame;
	}
	return total;
}

void recordResize(GLFWwindow *window, int width, int height) {
	bool *recreateGraphics = glfwGetWindowUserPointer(window);
	*recreateGraphics = true;
}

int main() {
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

