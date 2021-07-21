#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "util.h"

#define QF_GRAPHICS 0
#define QF_PRESENTATION 1
#define QF_LEN 2

#define STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT 8

struct GraphicsInstance {
    GLFWwindow *window;
    VkInstance instance;

    VkSurfaceKHR surface;

    VkPhysicalDevice dev_p;
    VkDevice dev;

    uint32_t qfi[QF_LEN];
    VkQueue gq, pq;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    void *stagingData;
    VkBuffer devBuffer;
    VkDeviceMemory devMemory;
};

struct Graphics {
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    VkSwapchainKHR swapchain;
    uint32_t swapchainImageCount;
    VkImage swapchainImages[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkImage textureImage;
    VkDeviceMemory textureMemory;

    VkImageView swapchainImageViews[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];
    VkFramebuffer swapchainFramebuffers[STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT];
    VkCommandPool commandPool;
    bool bufferAllocated;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
};

struct Vertex {
    float pos[2];
    float color[3];
    float circle[2];
};
#define VERTEX_BUFFER_LEN 65536
#define VERTEX_BUFFER_BYTES (VERTEX_BUFFER_LEN * sizeof(struct Vertex))

#define SPRITE_REGION_CAP 65536

void build_frame_data(
    size_t *vertex_count,
    struct Vertex* vertex_data,
    size_t *sprite_count,
    VkImageCopy *sprite_regions,
    int screen_width, int screen_height
);

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

void allocateMemory(
    struct GraphicsInstance *gi,
    VkMemoryRequirements memRequirements,
    VkMemoryPropertyFlags properties,
    VkDeviceMemory *bufferMemory
) {
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(gi->dev_p, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (
            (memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties
        ) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }
    if (vkAllocateMemory(gi->dev, &allocInfo, NULL, bufferMemory) != VK_SUCCESS)
    {
        printf("Failed to allocate memory!\n");
        exit(1);
    }
}

void createBuffer(
    struct GraphicsInstance *gi,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer *buffer,
    VkDeviceMemory *bufferMemory
) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(gi->dev, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
        printf("Failed to create vertex buffer!\n");
        exit(1);
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(gi->dev, *buffer, &memRequirements);

    allocateMemory(gi, memRequirements, properties, bufferMemory);
    vkBindBufferMemory(gi->dev, *buffer, *bufferMemory, 0);
}

typedef enum ImageMode {
    IMAGE_MODE_UNINITIALISED,
    IMAGE_MODE_WRITING,
    IMAGE_MODE_SAMPLING,
    IMAGE_MODE_READING,
    IMAGE_MODE_PRESENTING
} ImageMode;

void imageModeOptions(
    ImageMode mode,
    VkImageLayout *pLayout,
    VkAccessFlags *pAccess,
    VkPipelineStageFlagBits *pStage
) {
    VkImageLayout layout;
    VkAccessFlags access;
    VkPipelineStageFlagBits stage;

    switch (mode) {
    case IMAGE_MODE_UNINITIALISED:
        layout = VK_IMAGE_LAYOUT_UNDEFINED;
        access = 0;
        stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        break;
    case IMAGE_MODE_WRITING:
        layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        access = VK_ACCESS_TRANSFER_WRITE_BIT;
        stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case IMAGE_MODE_SAMPLING:
        layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        access = VK_ACCESS_SHADER_READ_BIT;
        stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;
    case IMAGE_MODE_READING:
        layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        access = VK_ACCESS_TRANSFER_READ_BIT;
        stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case IMAGE_MODE_PRESENTING:
        layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        access = 0;
        stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;
    }

    if (pLayout) *pLayout = layout;
    if (pAccess) *pAccess = access;
    if (pStage)  *pStage  = stage;
}

void transitionImageModeCmd(
    struct GraphicsInstance *gi,
    VkCommandBuffer commandBuffer,
    VkImage image,
    ImageMode oldMode,
    ImageMode newMode
) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    imageModeOptions(oldMode,
        &barrier.oldLayout, &barrier.srcAccessMask, &sourceStage);
    imageModeOptions(newMode,
        &barrier.newLayout, &barrier.dstAccessMask, &destinationStage);

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, 0,
        0, 0,
        1, &barrier
    );
}

struct GraphicsInstance createGraphicsInstance() {
    ///////////////
    // Constants

    const int DEFAULT_WIDTH = 800;
    const int DEFAULT_HEIGHT = 600;

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

    // @Cleanup maybe we should be using these more consistently
    struct GraphicsInstance g;
    struct GraphicsInstance *gi = &g;

    ///////////////////////////
    // Window Initialisation

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    g.window = glfwCreateWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Vulkan window",
            NULL, NULL);

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

    // vertex buffers
    {
        createBuffer(
            gi,
            VERTEX_BUFFER_BYTES,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &gi->stagingBuffer,
            &gi->stagingMemory
        );
        vkMapMemory(gi->dev, gi->stagingMemory, 0, VERTEX_BUFFER_BYTES, 0, &gi->stagingData);
        createBuffer(
            gi,
            VERTEX_BUFFER_BYTES,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &gi->devBuffer,
            &gi->devMemory
        );
    }

    return g;
}

struct Graphics createGraphics(struct GraphicsInstance *gi, int texWidth, int texHeight, int texDepth, uint8_t *tex) {
    struct Graphics g;
    g.bufferAllocated = false;

    // create swapchain
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gi->dev_p, gi->surface,
                &capabilities);

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = gi->surface;

        // image stuff
        {
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(gi->dev_p, gi->surface, &formatCount, NULL);
            VkSurfaceFormatKHR formats[formatCount];
            vkGetPhysicalDeviceSurfaceFormatsKHR(gi->dev_p, gi->surface, &formatCount, formats);
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
            unsigned width;
            unsigned height;
            glfwGetFramebufferSize(gi->window, (int*)&width, (int*)&height);

            if (width < capabilities.minImageExtent.width) {
                createInfo.imageExtent.width = capabilities.minImageExtent.width;
            } else if (width > capabilities.maxImageExtent.width) {
                createInfo.imageExtent.width = capabilities.maxImageExtent.width;
            } else {
                createInfo.imageExtent.width = width;
            }

            if (height < capabilities.minImageExtent.height) {
                createInfo.imageExtent.height = capabilities.minImageExtent.height;
            } else if (height > capabilities.maxImageExtent.height) {
                createInfo.imageExtent.height = capabilities.maxImageExtent.height;
            } else {
                createInfo.imageExtent.height = height;
            }
        }
        g.swapchainExtent = createInfo.imageExtent;
        if (capabilities.minImageCount == capabilities.maxImageCount) {
            createInfo.minImageCount = capabilities.maxImageCount;
        } else {
            createInfo.minImageCount = capabilities.minImageCount + 1;
        }
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // queuefamily stuff
        {
            bool unique = true;
            uint32_t x = gi->qfi[0];
            range(i, QF_LEN) {
                if (gi->qfi[i] != x) {
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
                createInfo.pQueueFamilyIndices = gi->qfi;
            }
        }

        // presentation stuff
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        {
            uint32_t presentModesCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(gi->dev_p, gi->surface,
                    &presentModesCount, NULL);
            VkPresentModeKHR presentModes[presentModesCount];
            vkGetPhysicalDeviceSurfacePresentModesKHR(gi->dev_p, gi->surface,
                    &presentModesCount, presentModes);

            range (i, presentModesCount) {
                if (presentModes[i] == VK_PRESENT_MODE_FIFO_KHR) {
                    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
                }
            }
        }

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(gi->dev, &createInfo, NULL, &g.swapchain) != VK_SUCCESS) {
            printf("failed to create swap chain!\n");
        }

        // we could combine these vulkan calls, but the validation layer gets
        // confused :)
        g.swapchainImageCount = 0;
        vkGetSwapchainImagesKHR(gi->dev, g.swapchain, &g.swapchainImageCount,
                NULL);
        if (g.swapchainImageCount > STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT)
        {
            printf("cannot handle more than %d swapchain images\n",
                    STRUCT_GRAPHICS_MAX_SWAPCHAIN_IMAGE_COUNT);
            exit(1);
        }
        vkGetSwapchainImagesKHR(gi->dev, g.swapchain, &g.swapchainImageCount,
                g.swapchainImages);
    }

    // create graphics pipeline
    {
        //shader stages
        VkShaderModule vert = createShaderModule(gi->dev, "vert.spv");
        VkShaderModule frag = createShaderModule(gi->dev, "frag.spv");

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
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(struct Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attributeDescriptions[3] = {};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = 0;
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = sizeof(float[2]);
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = sizeof(float[5]);
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = 3;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

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
                VK_COLOR_COMPONENT_B_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        if (vkCreatePipelineLayout(gi->dev, &pipelineLayoutInfo, NULL, &g.pipelineLayout) != VK_SUCCESS) {
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
        imageModeOptions(IMAGE_MODE_WRITING,
            &colorAttachment.finalLayout, NULL, NULL);

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


        if (vkCreateRenderPass(gi->dev, &renderPassInfo, NULL, &g.renderPass)
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

        if (vkCreateGraphicsPipelines(gi->dev, VK_NULL_HANDLE, 1, &pipelineInfo,
                NULL, &g.graphicsPipeline) != VK_SUCCESS)
        {
            printf("failed to create graphics pipeline!\n");
        }

        vkDestroyShaderModule(gi->dev, vert, NULL);
        vkDestroyShaderModule(gi->dev, frag, NULL);
    }

    // create command pool
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = gi->qfi[QF_GRAPHICS];
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        if (vkCreateCommandPool(gi->dev, &poolInfo, NULL, &g.commandPool) != VK_SUCCESS) {
            printf("failed to create command pool!");
        }
    }

    // create font texture
    {
        // staging buffer
        VkDeviceSize imageSize = texWidth * texHeight * texDepth * 4;

        VkBuffer imageStagingBuffer;
        VkDeviceMemory imageStagingMemory;
        createBuffer(
            gi,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &imageStagingBuffer,
            &imageStagingMemory
        );

        void* data;
        vkMapMemory(gi->dev, imageStagingMemory, 0, imageSize, 0, &data);
        memcpy(data, tex, (unsigned)imageSize);
        vkUnmapMemory(gi->dev, imageStagingMemory);

        // image creation
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.extent.width = texWidth;
        imageInfo.extent.height = texHeight;
        imageInfo.extent.depth = texDepth;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(gi->dev, &imageInfo, 0, &g.textureImage)
            != VK_SUCCESS)
        {
            printf("failed to create image\n");
            exit(1);
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(gi->dev, g.textureImage, &memRequirements);

        allocateMemory(
            gi,
            memRequirements,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &g.textureMemory
        );

        vkBindImageMemory(gi->dev, g.textureImage, g.textureMemory, 0);

        // image transfer
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = g.commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(gi->dev, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        transitionImageModeCmd(
            gi,
            commandBuffer,
            g.textureImage,
            IMAGE_MODE_UNINITIALISED,
            IMAGE_MODE_WRITING
        );

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = (VkOffset3D){0, 0, 0};
        region.imageExtent = (VkExtent3D){texWidth, texHeight, texDepth};

        vkCmdCopyBufferToImage(
            commandBuffer,
            imageStagingBuffer,
            g.textureImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        transitionImageModeCmd(
            gi,
            commandBuffer,
            g.textureImage,
            IMAGE_MODE_WRITING,
            IMAGE_MODE_READING
        );

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(gi->gq, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(gi->gq);

        vkFreeCommandBuffers(gi->dev, g.commandPool, 1, &commandBuffer);

        vkDestroyBuffer(gi->dev, imageStagingBuffer, 0);
        vkFreeMemory(gi->dev, imageStagingMemory, 0);
    }

    // create views, framebuffers, vertexbuffer, commandbuffers
    range (i, g.swapchainImageCount) {
        // image view
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = g.swapchainImageFormat;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.image = g.swapchainImages[i];

        if (vkCreateImageView(gi->dev, &createInfo, NULL,
                &g.swapchainImageViews[i]) != VK_SUCCESS)
        {
            printf("failed to create image view!\n");
            exit(1);
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

        if (vkCreateFramebuffer(gi->dev, &framebufferInfo, NULL,
                &g.swapchainFramebuffers[i]) != VK_SUCCESS)
        {
            printf("failed to create framebuffer!\n");
            exit(1);
        }
    }

    // create semaphores
    {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(gi->dev, &semaphoreInfo, NULL,
                &g.imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(gi->dev, &semaphoreInfo, NULL,
                &g.renderFinishedSemaphore) != VK_SUCCESS)
        {
            printf("failed to create semaphores!\n");
        }
    }

    return g;
}

bool drawFrame(struct GraphicsInstance *gi, struct Graphics *g) {
    // let previous frame finish
    vkQueueWaitIdle(gi->pq);
    if (g->bufferAllocated) {
        vkFreeCommandBuffers(gi->dev, g->commandPool, 1, &g->commandBuffer);
        g->bufferAllocated = false;
    }

    // copy vertex data
    size_t vertex_count;
    size_t sprite_count;
    static VkImageCopy sprite_regions[SPRITE_REGION_CAP];
    build_frame_data(
        &vertex_count,
        (struct Vertex*)gi->stagingData,
        &sprite_count,
        sprite_regions,
        g->swapchainExtent.width,
        g->swapchainExtent.height
    );
    if (vertex_count > VERTEX_BUFFER_LEN) {
        printf("Vertex count exceeds buffer length\n");
        exit(1);
    }
    if (sprite_count > SPRITE_REGION_CAP) {
        printf("Sprite count exceeds buffer length\n");
        exit(1);
    }

    // draw frame
    uint32_t imageIndex;
    VkResult result;
    result = vkAcquireNextImageKHR(gi->dev, g->swapchain, UINT64_MAX,
            g->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("failed to acquire swap chain image!\n");
        exit(1);
    }

    // command buffer
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = g->commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(gi->dev, &allocInfo, &g->commandBuffer) != VK_SUCCESS) {
            printf("failed to allocate command buffer!");
            exit(1);
        }
        g->bufferAllocated = true;
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;

        if (vkBeginCommandBuffer(g->commandBuffer, &beginInfo) != VK_SUCCESS) {
            printf("failed to begin recording command buffer!\n");
            exit(1);
        }

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = VERTEX_BUFFER_BYTES;
        vkCmdCopyBuffer(g->commandBuffer, gi->stagingBuffer, gi->devBuffer, 1, &copyRegion);

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = g->renderPass;
        renderPassInfo.framebuffer = g->swapchainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = (VkOffset2D) {0, 0};
        renderPassInfo.renderArea.extent = g->swapchainExtent;

        VkClearValue clearColor;
        clearColor.color.float32[0] = 0.0f;
        clearColor.color.float32[1] = 0.0f;
        clearColor.color.float32[2] = 0.0f;
        clearColor.color.float32[3] = 1.0f;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(g->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(g->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g->graphicsPipeline);

        VkBuffer vertexBuffers[] = {gi->devBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(g->commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdDraw(g->commandBuffer, vertex_count, 1, 0, 0);
        vkCmdEndRenderPass(g->commandBuffer);

        vkCmdCopyImage(
            g->commandBuffer,
            g->textureImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            g->swapchainImages[imageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            sprite_count,
            sprite_regions
        );

        transitionImageModeCmd(
            gi,
            g->commandBuffer,
            g->swapchainImages[imageIndex],
            IMAGE_MODE_WRITING,
            IMAGE_MODE_PRESENTING
        );

        if (vkEndCommandBuffer(g->commandBuffer) != VK_SUCCESS) {
            printf("failed to record command buffer!\n");
            exit(1);
        }
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {g->imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] =
        {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g->commandBuffer;

    VkSemaphore signalSemaphores[] = {g->renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(gi->gq, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        printf("failed to submit draw command buffer!\n");
        exit(1);
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {g->swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;

    result = vkQueuePresentKHR(gi->pq, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false;
    }
    if (result != VK_SUCCESS) {
        printf("failed to present swap chain image!\n");
        exit(1);
    }
    return true;
}


void destroyGraphics(struct GraphicsInstance *gi, struct Graphics *g) {
    vkDeviceWaitIdle(gi->dev);

    vkDestroySemaphore(gi->dev, g->renderFinishedSemaphore, NULL);
    vkDestroySemaphore(gi->dev, g->imageAvailableSemaphore, NULL);

    // @Performance wasteful?
    vkDestroyCommandPool(gi->dev, g->commandPool, NULL);

    vkDestroyImage(gi->dev, g->textureImage, NULL);
    vkFreeMemory(gi->dev, g->textureMemory, NULL);

    range (i, g->swapchainImageCount) {
        vkDestroyFramebuffer(gi->dev, g->swapchainFramebuffers[i], NULL);
        vkDestroyImageView(gi->dev, g->swapchainImageViews[i], NULL);
    }

    vkDestroyPipeline(gi->dev, g->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(gi->dev, g->pipelineLayout, NULL);
    vkDestroyRenderPass(gi->dev, g->renderPass, NULL);

    vkDestroySwapchainKHR(gi->dev, g->swapchain, NULL);
}

void destroyGraphicsInstance(struct GraphicsInstance *gi) {
    vkDestroyBuffer(gi->dev, gi->devBuffer, NULL);
    vkFreeMemory(gi->dev, gi->devMemory, NULL);

    vkUnmapMemory(gi->dev, gi->stagingMemory);
    vkDestroyBuffer(gi->dev, gi->stagingBuffer, NULL);
    vkFreeMemory(gi->dev, gi->stagingMemory, NULL);

    vkDestroyDevice(gi->dev, NULL);

    vkDestroySurfaceKHR(gi->instance, gi->surface, NULL);

    vkDestroyInstance(gi->instance, NULL);

    glfwDestroyWindow(gi->window);

    glfwTerminate();
}

