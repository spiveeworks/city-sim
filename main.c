#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define array_ptr(T) struct {size_t len; (T) *data;}

#define range(i, max) for (size_t (i) = 0; (i) < (max); ++(i))

struct Graphics {
	GLFWwindow *window;
	VkInstance instance;

	VkPhysicalDevice dev_p;
	VkDevice dev;
	VkQueue gq;
};

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

			range (i, VALIDATION_LAYERS_LEN) {
				bool layerFound = false;
				range (j, layerCount) {
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

	// get physical/logical device
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(g.instance, &deviceCount, NULL);

		if (!deviceCount) {
			printf("no devices with vulkan support\n");
			exit(1);
		}

		VkPhysicalDevice devices[deviceCount];
		vkEnumeratePhysicalDevices(g.instance, &deviceCount, devices);

		g.dev_p = VK_NULL_HANDLE;

#define QF_GRAPHICS 0
#define QF_LEN 1
		struct {
			uint64_t flags; 
			uint64_t indices[QF_LEN];
		} qf;
		const uint64_t QF_COMPLETE = ((1 << QF_LEN) - 1);

		range (i, deviceCount) {
			g.dev_p = devices[i];
			// check queue families
			uint32_t qf_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(g.dev_p, &qf_count, NULL);

			VkQueueFamilyProperties qfs[qf_count];
			vkGetPhysicalDeviceQueueFamilyProperties(g.dev_p, &qf_count, qfs);
			qf.flags = 0;
			range (j, qf_count) {
				if (qfs[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					qf.flags |= 1 << QF_GRAPHICS;
					qf.indices[QF_GRAPHICS] = j;
				}
			}

			if (qf.flags != QF_COMPLETE) {
				continue;
			}

			// all checks succeeded so stop looking
			break;
		}

		if (g.dev_p == VK_NULL_HANDLE) {
			printf("no suitable devices");
		}

		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = qf.indices[QF_GRAPHICS];
		queueCreateInfo.queueCount = 1;
		float queuePriority = 1.0f;

		queueCreateInfo.pQueuePriorities = &queuePriority;
		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = 1;
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = 0;
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = VALIDATION_LAYERS_LEN;
			createInfo.ppEnabledLayerNames = validationLayers;
		} else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(g.dev_p, &createInfo, NULL, &g.dev) != VK_SUCCESS) {
			printf("failed to create logical device!\n");
		}

		vkGetDeviceQueue(g.dev, qf.indices[QF_GRAPHICS], 0, &g.gq);
	}

	////////////////
	// Event Loop

	while(!glfwWindowShouldClose(g.window)) {
		glfwPollEvents();
	}

	//////////////////////
	// graphics cleanup

	vkDestroyDevice(g.dev, NULL);

	vkDestroyInstance(g.instance, NULL);

	glfwDestroyWindow(g.window);

	glfwTerminate();

	return 0;
}
