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

	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	VkImageView swapchainImageViews[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];
	VkFramebuffer swapchainFramebuffers[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffers[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
};

VkShaderModule createShaderModule(VkDevice dev, char* filename) {
	size_t size = 0;
	FILE *f = fopen(filename, "reb");
	if (f == NULL) 
	{ 
		printf("Failed to open shader file: %s\n", filename);
		exit(1);
	} 
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint32_t buffer[size / sizeof(uint32_t) + 1];
	if (size != fread(buffer, sizeof(char), size, f))
	{
		printf("Failed to read shader file: %s\n", filename);
		exit(1);
	}
	fclose(f);

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = size;
	createInfo.pCode = buffer;

	VkShaderModule result;
	if (vkCreateShaderModule(dev, &createInfo, NULL, &result) != VK_SUCCESS) {
		printf("failed to create shader module from file: %s\n", filename);
		exit(1);
	}
	return result;
}

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

	// create graphics pipeline
	{
		//shader stages
		VkShaderModule vert = createShaderModule(g.dev, "vert.spv");
		VkShaderModule frag = createShaderModule(g.dev, "frag.spv");

		VkPipelineShaderStageCreateInfo vertStageInfo = {};
		vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageInfo.module = vert;
		vertStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragStageInfo = {};
		fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageInfo.module = frag;
		fragStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[2] =
			{vertStageInfo, fragStageInfo};

		// fixed function stages
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) g.swapchainExtent.width;
		viewport.height = (float) g.swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {{0, 0}, g.swapchainExtent};

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkDynamicState dynamicStates[2] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH // really?
		};
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		// layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		if (vkCreatePipelineLayout(g.dev, &pipelineLayoutInfo, NULL, &g.pipelineLayout) != VK_SUCCESS) {
			printf("failed to create pipeline layout!\n");
			exit(1);
		}

		// render pass
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = g.swapchainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;


		if (vkCreateRenderPass(g.dev, &renderPassInfo, NULL, &g.renderPass)
				!= VK_SUCCESS)
		{
			printf("failed to create render pass!\n");
		}

		// pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = NULL;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = NULL;
		pipelineInfo.layout = g.pipelineLayout;
		pipelineInfo.renderPass = g.renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(g.dev, VK_NULL_HANDLE, 1, &pipelineInfo,
				NULL, &g.graphicsPipeline) != VK_SUCCESS)
		{
			printf("failed to create graphics pipeline!\n");
		}

		vkDestroyShaderModule(g.dev, vert, NULL);
		vkDestroyShaderModule(g.dev, frag, NULL);
	}

	// create command pool
	{
		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = g.qfi[QF_GRAPHICS];
		poolInfo.flags = 0;

		if (vkCreateCommandPool(g.dev, &poolInfo, NULL, &g.commandPool) != VK_SUCCESS) {
			printf("failed to create command pool!");
		}

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = g.commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) g.swapchainImageCount;

		if (vkAllocateCommandBuffers(g.dev, &allocInfo, g.commandBuffers) != VK_SUCCESS) {
			printf("failed to allocate command buffers!");
			exit(1);
		}
	}

	// create views, framebuffers, commandbuffers
	range (i, g.swapchainImageCount) {
		// image view
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
		createInfo.image = g.swapchainImages[i];

		if (vkCreateImageView(g.dev, &createInfo, NULL,
				&g.swapchainImageViews[i]) != VK_SUCCESS)
		{
			printf("failed to create image view!\n");
		}

		// framebuffer
		VkImageView attachments[] = {
			g.swapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = g.renderPass;
		framebufferInfo.width = g.swapchainExtent.width;
		framebufferInfo.height = g.swapchainExtent.height;
		framebufferInfo.layers = 1;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;

		if (vkCreateFramebuffer(g.dev, &framebufferInfo, NULL,
				&g.swapchainFramebuffers[i]) != VK_SUCCESS)
		{
			printf("failed to create framebuffer!\n");
			exit(1);
		}

		// command buffer
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = NULL;

		if (vkBeginCommandBuffer(g.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
			printf("failed to begin recording command buffer!\n");
			exit(1);
		}

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = g.renderPass;
		renderPassInfo.framebuffer = g.swapchainFramebuffers[i];

		renderPassInfo.renderArea.offset = (VkOffset2D) {0, 0};
		renderPassInfo.renderArea.extent = g.swapchainExtent;

		VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(g.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(g.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g.graphicsPipeline);
		vkCmdDraw(g.commandBuffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(g.commandBuffers[i]);

		if (vkEndCommandBuffer(g.commandBuffers[i]) != VK_SUCCESS) {
			printf("failed to record command buffer!\n");
			exit(1);
		}
	}

	// create semaphores
	{
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(g.dev, &semaphoreInfo, NULL,
				&g.imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(g.dev, &semaphoreInfo, NULL,
				&g.renderFinishedSemaphore) != VK_SUCCESS)
		{
			printf("failed to create semaphores!\n");
		}
	}

	////////////////
	// Event Loop

	while(!glfwWindowShouldClose(g.window)) {
		glfwPollEvents();

		// let previous frame finish
		vkQueueWaitIdle(g.pq);

		// draw frame
		uint32_t imageIndex;
		vkAcquireNextImageKHR(g.dev, g.swapchain, UINT64_MAX,
				g.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = {g.imageAvailableSemaphore};
		VkPipelineStageFlags waitStages[] =
			{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &g.commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = {g.renderFinishedSemaphore};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(g.gq, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			printf("failed to submit draw command buffer!\n");
			exit(1);
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[] = {g.swapchain};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = NULL;

		vkQueuePresentKHR(g.pq, &presentInfo);
	}

    vkDeviceWaitIdle(g.dev);

	//////////////////////
	// graphics cleanup

	vkDestroySemaphore(g.dev, g.renderFinishedSemaphore, NULL);
	vkDestroySemaphore(g.dev, g.imageAvailableSemaphore, NULL);

	vkDestroyCommandPool(g.dev, g.commandPool, NULL);

	range (i, g.swapchainImageCount) {
		vkDestroyFramebuffer(g.dev, g.swapchainFramebuffers[i], NULL);
		vkDestroyImageView(g.dev, g.swapchainImageViews[i], NULL);
	}

	vkDestroyPipeline(g.dev, g.graphicsPipeline, NULL);
	vkDestroyPipelineLayout(g.dev, g.pipelineLayout, NULL);
	vkDestroyRenderPass(g.dev, g.renderPass, NULL);

	vkDestroySwapchainKHR(g.dev, g.swapchain, NULL);
	vkDestroyDevice(g.dev, NULL);

	vkDestroySurfaceKHR(g.instance, g.surface, NULL);

	vkDestroyInstance(g.instance, NULL);

	glfwDestroyWindow(g.window);

	glfwTerminate();

	return 0;
}
