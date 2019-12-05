#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define array_ptr(T) struct {size_t len; (T) *data;}

#define range(i, max) for (size_t (i) = 0; (i) < (max); ++(i))

#define QF_GRAPHICS 0
#define QF_PRESENTATION 1
#define QF_LEN 2

#define STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT 8

struct Graphics {
	GLFWwindow *window;
	VkInstance instance;

	VkSurfaceKHR surface;

	VkPhysicalDevice dev_p;
	VkDevice dev;

	uint32_t qfi[QF_LEN];
	VkQueue gq, pq;

	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;
	VkSwapchainKHR swapchain;
	uint32_t swapchainImageCount;
	VkImage swapchainImages[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];
	VkImageView swapchainImageViews[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];
};

int main() {
	///////////////
	// Constants

	const int WIDTH = 800;
	const int HEIGHT = 600;

#ifdef NDEBUG
	size_t VALIDATION_LAYERS_LEN = 0;
	const char *validationLayers[VALIDATION_LAYERS_LEN];
#else
	printf("Running debug build.\n");
	size_t VALIDATION_LAYERS_LEN = 1;
	const char *validationLayers[VALIDATION_LAYERS_LEN];
	validationLayers[0] = "VK_LAYER_KHRONOS_validation";
#endif

	const size_t REQUIRED_EXT_COUNT = 1;
	const char* required_exts[REQUIRED_EXT_COUNT];
	required_exts[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

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

#ifndef NDEBUG
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
#endif

		createInfo.enabledLayerCount = VALIDATION_LAYERS_LEN;
		createInfo.ppEnabledLayerNames = validationLayers;

		if (vkCreateInstance(&createInfo, NULL, &g.instance) != VK_SUCCESS) {
			printf("failed to create instance!\n");
			exit(1);
		}
	}

	// create surface
	if (glfwCreateWindowSurface(g.instance, g.window, NULL, &g.surface)
			!= VK_SUCCESS)
	{
		printf("failed to create window surface!\n");
		exit(1);
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

		range (i, deviceCount) {
			g.dev_p = devices[i];
			// check queue families
			uint32_t qf_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(g.dev_p, &qf_count, NULL);

			VkQueueFamilyProperties qfs[qf_count];
			vkGetPhysicalDeviceQueueFamilyProperties(g.dev_p, &qf_count, qfs);
			if (QF_LEN >= 8) {
				printf("queuefamily selection currently assumes qfi_len < 8");
			}
			uint8_t flags = 0;
			range (j, qf_count) {
				if (qfs[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					flags |= 1 << QF_GRAPHICS;
					g.qfi[QF_GRAPHICS] = j;
				}

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(g.dev_p, j, g.surface,
						&presentSupport);
				if (presentSupport) {
					flags |= 1 << QF_PRESENTATION;
					g.qfi[QF_PRESENTATION] = j;
				}
			}

			if (flags != ((1 << QF_LEN) - 1)) {
				continue;
			}

			// check extension support
			uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(g.dev_p, NULL,
					&extensionCount, NULL);

			VkExtensionProperties availableExtensions[extensionCount];
			vkEnumerateDeviceExtensionProperties(g.dev_p, NULL,
					&extensionCount, availableExtensions);

			bool allExts = true;
			range (i, REQUIRED_EXT_COUNT) {
				bool thisExt = false;
				range (j, extensionCount) {
					if (strcmp(required_exts[i], availableExtensions[j].extensionName) == 0) {
						thisExt = true;
						break;
					}
				}
				if (!thisExt) {
					allExts = false;
					break;
				}
			}
			if (!allExts) {
				continue;
			}

			// check swapchain support (ONLY if its extension is supported)
			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(g.dev_p, g.surface, &formatCount,
					NULL);
			if (formatCount == 0) {
				continue;
			}
			uint32_t presentModesCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(g.dev_p, g.surface,
					&presentModesCount, NULL);
			if (presentModesCount == 0) {
				continue;
			}

			// all checks succeeded so stop looking
			break;
		}

		if (g.dev_p == VK_NULL_HANDLE) {
			printf("no suitable devices");
		}

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo[QF_LEN] = {};
		size_t unique = 0;
		range (i, QF_LEN) {
			bool is_unique = true;
			range (j, i) {
				if (g.qfi[j] == g.qfi[i]) {
					is_unique = false;
					break;
				}
			}
			if (is_unique) {
				queueCreateInfo[unique].sType =
					VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo[unique].queueFamilyIndex = g.qfi[i];
				queueCreateInfo[unique].queueCount = 1;
				queueCreateInfo[unique].pQueuePriorities = &queuePriority;
				++unique;
			}
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = unique;
		createInfo.pQueueCreateInfos = queueCreateInfo;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = REQUIRED_EXT_COUNT;
		createInfo.ppEnabledExtensionNames = required_exts;
		createInfo.enabledLayerCount = VALIDATION_LAYERS_LEN;
		createInfo.ppEnabledLayerNames = validationLayers;

		if (vkCreateDevice(g.dev_p, &createInfo, NULL, &g.dev) != VK_SUCCESS) {
			printf("failed to create logical device!\n");
		}

		vkGetDeviceQueue(g.dev, g.qfi[QF_GRAPHICS], 0, &g.gq);
		vkGetDeviceQueue(g.dev, g.qfi[QF_PRESENTATION], 0, &g.pq);
	}

	// create swapchain
	{
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g.dev_p, g.surface,
				&capabilities);

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = g.surface;

		// image stuff
		{
			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(g.dev_p, g.surface, &formatCount, NULL);
			VkSurfaceFormatKHR formats[formatCount];
			vkGetPhysicalDeviceSurfaceFormatsKHR(g.dev_p, g.surface, &formatCount, formats);
			size_t format = 0;
			range (i, formatCount) {
				if (
					formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
					&& formats[i].format == VK_FORMAT_B8G8R8A8_UNORM
				) {
					format = i;
					break;
				}
			}
			createInfo.imageFormat = formats[format].format;
			createInfo.imageColorSpace = formats[format].colorSpace;
		}
		g.swapchainImageFormat = createInfo.imageFormat;
		if (capabilities.currentExtent.width != UINT32_MAX) {
			createInfo.imageExtent = capabilities.currentExtent;
		} else {
			if (WIDTH < capabilities.minImageExtent.width) {
				createInfo.imageExtent.width = capabilities.minImageExtent.width;
			} else if (WIDTH > capabilities.maxImageExtent.width) {
				createInfo.imageExtent.width = capabilities.maxImageExtent.width;
			} else {
				createInfo.imageExtent.width = WIDTH;
			}

			if (HEIGHT < capabilities.minImageExtent.height) {
				createInfo.imageExtent.height = capabilities.minImageExtent.height;
			} else if (HEIGHT > capabilities.maxImageExtent.height) {
				createInfo.imageExtent.height = capabilities.maxImageExtent.height;
			} else {
				createInfo.imageExtent.height = HEIGHT;
			}
		}
		g.swapchainExtent = createInfo.imageExtent;
		if (capabilities.minImageCount == capabilities.maxImageCount) {
			createInfo.minImageCount = capabilities.maxImageCount;
		} else {
			createInfo.minImageCount = capabilities.minImageCount + 1;
		}
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// queuefamily stuff
		{
			bool unique = true;
			uint32_t x = g.qfi[0];
			range(i, QF_LEN) {
				if (g.qfi[i] != x) {
					unique = false;
					break;
				}
			}
			if (unique) {
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0; // Optional
				createInfo.pQueueFamilyIndices = NULL; // Optional
			} else if (QF_LEN != 2) {
					printf("swapchain sharing mode only configured for 2 queue families\n");
					exit(1);
			} else {
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = g.qfi;
			}
		}

		// presentation stuff
		createInfo.preTransform = capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.clipped = VK_TRUE;
		createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		{
			uint32_t presentModesCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(g.dev_p, g.surface,
					&presentModesCount, NULL);
			VkPresentModeKHR presentModes[presentModesCount];
			vkGetPhysicalDeviceSurfacePresentModesKHR(g.dev_p, g.surface,
					&presentModesCount, presentModes);

			range (i, presentModesCount) {
				if (presentModes[i] == VK_PRESENT_MODE_FIFO_KHR) {
					createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
				}
			}
		}

		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(g.dev, &createInfo, NULL, &g.swapchain) != VK_SUCCESS) {
			printf("failed to create swap chain!\n");
		}

		// we could combine these vulkan calls, but the validation layer gets
		// confused :)
		g.swapchainImageCount = 0;
		vkGetSwapchainImagesKHR(g.dev, g.swapchain, &g.swapchainImageCount,
				NULL);
		if (g.swapchainImageCount > STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT)
		{
			printf("cannot handle more than %d swapchain images\n",
					STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT);
			exit(1);
		}
		vkGetSwapchainImagesKHR(g.dev, g.swapchain, &g.swapchainImageCount,
				g.swapchainImages);
	}

	// create image views
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = g.swapchainImageFormat;

		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		range (i, g.swapchainImageCount) {
			createInfo.image = g.swapchainImages[i];

			if (vkCreateImageView(g.dev, &createInfo, NULL,
					&g.swapchainImageViews[i]) != VK_SUCCESS)
			{
				printf("failed to create image views!\n");
			}
		}
	}

	////////////////
	// Event Loop

	while(!glfwWindowShouldClose(g.window)) {
		glfwPollEvents();
	}

	//////////////////////
	// graphics cleanup

	range (i, g.swapchainImageCount) {
		vkDestroyImageView(g.dev, g.swapchainImageViews[i], NULL);
	}

	vkDestroySwapchainKHR(g.dev, g.swapchain, NULL);
	vkDestroyDevice(g.dev, NULL);

	vkDestroySurfaceKHR(g.instance, g.surface, NULL);

	vkDestroyInstance(g.instance, NULL);

	glfwDestroyWindow(g.window);

	glfwTerminate();

	return 0;
}
