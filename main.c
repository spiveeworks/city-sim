#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Graphics {
	GLFWwindow *window;
	VkInstance instance;
};

#define array_ptr(T) struct {size_t len; (T) *data;}



int main() {

	///////////////
	// Constants

	const int WIDTH = 800;
	const int HEIGHT = 600;

	const int VALIDATION_LAYERS_LEN = 1;
	const char *validationLayers[VALIDATION_LAYERS_LEN];
	validationLayers[0] = "VK_LAYER_KHRONOS_validation";

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	struct Graphics g;

	///////////////////////////
	// Window Initialisation

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	g.window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", NULL, NULL);

	///////////////////////////
	// Vulkan Initialisation

	// create instance
	{
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;

		if (enableValidationLayers) {
			// @Cleanup why do we do this check when vkCreateInstance checks already?
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, NULL);

			VkLayerProperties layers[layerCount];

			vkEnumerateInstanceLayerProperties(&layerCount, layers);

			for (int i = 0; i < VALIDATION_LAYERS_LEN; i++) {
				bool layerFound = false;
				for (int j = 0; j < layerCount; j++) {
					if (strcmp(validationLayers[i], layers[j].layerName) == 0) {
						layerFound = true;
						break;
					}
				}
				if (!layerFound) {
					printf("validation layer not found: %s\n", validationLayers[i]);
					exit(1);
				}
			}

			createInfo.enabledLayerCount = VALIDATION_LAYERS_LEN;
			createInfo.ppEnabledLayerNames = validationLayers;
		} else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateInstance(&createInfo, NULL, &g.instance) != VK_SUCCESS) {
			printf("failed to create instance!\n");
			exit(1);
		}
	}

	////////////////
	// Event Loop

	while(!glfwWindowShouldClose(g.window)) {
		glfwPollEvents();
	}

	////////////////////////
	// Window Destruction

	vkDestroyInstance(g.instance, NULL);

	glfwDestroyWindow(g.window);

	glfwTerminate();

	return 0;
}
