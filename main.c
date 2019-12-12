#include "graphics.h"

int main() {
	struct Graphics g = createGraphics();

	while(!glfwWindowShouldClose(g.window)) {
		glfwPollEvents();

		drawFrame(&g);
	}

	destroyGraphics(&g);

	return 0;
}

