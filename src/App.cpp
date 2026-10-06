#include <SDL3/SDL_vulkan.h>

#include <shaderc/env.h>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <shaderc/status.h>

#include <iostream>
#include <vector>

#include "App.hpp"
#include "vulkan/vulkan_core.h"

bool error(const char* msg)
{
	std::cerr << msg << "\n";
	return false;
}

VkBool32 debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       pUserData
	)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		std::cerr << "VK error: " << pCallbackData->pMessage << "\n";
	return (VK_SUCCESS);
}

bool App::createVulkanInstance()
{
	if (volkInitialize() != VK_SUCCESS)
		return error("cannot start volk");

	VkApplicationInfo app_info 
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "my vulkan app :3",
		.apiVersion = App::VK_VERSION,
	};

	u32 sdl_extentions_count;
	const char *const *sdl_extentions = SDL_Vulkan_GetInstanceExtensions(&sdl_extentions_count);
	if (sdl_extentions == nullptr)
		return error("sdl cannot get extentions");
	std::vector<const char *> extentions
	{
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
	};
	for (u32 i = 0; i < sdl_extentions_count; ++i)
		extentions.push_back(sdl_extentions[i]);

	std::vector<const char *> layers
	{
		"VK_LAYER_KHRONOS_validation",
	};

	VkDebugUtilsMessengerCreateInfoEXT debug_info
	{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = &debugCallback,
	};

	VkInstanceCreateInfo create_info
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = &debug_info,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = (u32)layers.size(),
		.ppEnabledLayerNames = layers.data(),
		.enabledExtensionCount = (u32)extentions.size(),
		.ppEnabledExtensionNames = extentions.data()
	};

	if (vkCreateInstance(&create_info, nullptr, &vk_instance) != VK_SUCCESS)
		return false;

	volkLoadInstance(vk_instance);
	return true;
}

bool App::getPhysicalDevice()
{
	u32 devices_count = 0;
	vkEnumeratePhysicalDevices(vk_instance, &devices_count, nullptr);
	if (devices_count < 1)
		return error("no physical device can be found");
	std::vector<VkPhysicalDevice> devices(devices_count);
	if (vkEnumeratePhysicalDevices(vk_instance, &devices_count, devices.data()) != VK_SUCCESS)
		return error("no physical devices can be retrieved");

	
	std::vector<VkPhysicalDeviceProperties2> device_props(devices_count, 
		{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2});
	VkPhysicalDevice main_device = devices[0];
	u16 i = 0;
	for (; i < devices_count; ++i)
	{
		VkPhysicalDeviceProperties2* props = device_props.data() + i;
		vkGetPhysicalDeviceProperties2(devices[i], props);
		std::cout << "physical device: " << props->properties.deviceName << " " << props->properties.deviceType << "\n";
		if (props->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			main_device = devices[i];
			break;
		}
	}
	std::cout << "the choosen physical device is: " << device_props[i].properties.deviceName << "\n";
	vk_physical_device = main_device;

	u32 format_count = 0;
	VkPhysicalDeviceSurfaceInfo2KHR surface_info
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		.surface = vk_surface,
	};
	vkGetPhysicalDeviceSurfaceFormats2KHR(vk_physical_device, &surface_info, &format_count, nullptr);
	if (format_count < 1)
		return error("no physical devices surface format can be found");
	std::vector<VkSurfaceFormat2KHR> formats_supported {format_count, {.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR}};
	if (vkGetPhysicalDeviceSurfaceFormats2KHR(vk_physical_device, &surface_info, &format_count, formats_supported.data()) != VK_SUCCESS)
		return error("no physical devices surface format can be retrieved");

	for (const VkSurfaceFormat2KHR& format_supported : formats_supported)
	{
		if (format_supported.surfaceFormat.format == SWAPCHAIN_FORMAT)
			return true;
	}
	return error("physical device surface format not supported");
}

bool App::getGraphicsQueue()
{
	u32 queues_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(vk_physical_device, &queues_count, nullptr);
	if (queues_count < 1)
		return error("no queue family can be found");

	std::vector<VkQueueFamilyProperties2> queues_props(queues_count, {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
	vkGetPhysicalDeviceQueueFamilyProperties2(vk_physical_device, &queues_count, queues_props.data());

	bool found = false;
	for (u32 i = 0; i < queues_count; ++i)
	{
		VkBool32 is_supported = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(vk_physical_device, i, vk_surface, &is_supported);
		if (is_supported == VK_FALSE)
			continue;

		const VkQueueFamilyProperties2& props = queues_props[i];
		if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			queue_fam_idx = i;
			found = true;
			break;
		}
	}
	if (!found)
		return error("no valid graphic queue found");
	//TODO reput like it was befor ???
	return true;
}

bool App::createDevice()
{
	VkPhysicalDeviceVulkan14Features	supported_features_14{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, .pNext = nullptr };
	VkPhysicalDeviceVulkan13Features	supported_features_13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &supported_features_14 };
	VkPhysicalDeviceVulkan12Features	supported_features_12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &supported_features_13 };
	VkPhysicalDeviceVulkan11Features	supported_features_11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = &supported_features_12 };
	VkPhysicalDeviceFeatures2			supported_features_10{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supported_features_11 };
	vkGetPhysicalDeviceFeatures2(vk_physical_device, &supported_features_10);

	if (!supported_features_13.dynamicRendering || !supported_features_13.synchronization2 ||
		!supported_features_12.timelineSemaphore)
		return error("Physical device doesn't meet the feature requirements");

	VkPhysicalDeviceVulkan14Features features_14
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
		.pNext = nullptr,
	};
	VkPhysicalDeviceVulkan13Features features_13
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &features_14,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE,
	};
	VkPhysicalDeviceVulkan12Features features_12
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &features_13,
		.timelineSemaphore = VK_TRUE
	};
	VkPhysicalDeviceVulkan11Features features_11
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &features_12,
	};
	VkPhysicalDeviceFeatures2 features_10
	 {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &features_11
	};

	const std::vector<f32> queues_priorities {1.0f};
	u32 queues_size = queues_priorities.size();
	VkDeviceQueueCreateInfo queue_info
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queue_fam_idx,
		.queueCount = queues_size,
		.pQueuePriorities = queues_priorities.data(),
	};

	const std::vector<const char *> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	u32 extensions_size = device_extensions.size();
	VkDeviceCreateInfo device_info
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features_10,
		.queueCreateInfoCount = queues_size,
		.pQueueCreateInfos = &queue_info,
		.enabledExtensionCount = extensions_size,
		.ppEnabledExtensionNames = device_extensions.data(),
	};

	if (vkCreateDevice(vk_physical_device, &device_info, nullptr, &vk_device) != VK_SUCCESS)
		return error("cannot create device");

	vkGetDeviceQueue(vk_device, queue_fam_idx, 0, &vk_queue);
	return true;
}

bool App::initialiseVMA()
{
	VmaVulkanFunctions func_info {};
	VmaAllocatorCreateInfo alloc_info
	{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = vk_physical_device,
		.device = vk_device,
		.pVulkanFunctions = &func_info,
		.instance = vk_instance,
		.vulkanApiVersion = App::VK_VERSION,
	};

	if (vmaImportVulkanFunctionsFromVolk(&alloc_info, &func_info) != VK_SUCCESS)
		return error("vma cannot get vulkan func from volk");
	if (vmaCreateAllocator(&alloc_info, &vma_allocator) != VK_SUCCESS)
		return error("vma cannot ");
	return true;
}

bool App::createSwapchain(const u32 width, const u32 height)
{
	swapchain_width = width;
	swapchain_height = height;

	VkSurfaceCapabilities2KHR surface_capabilities
	{
		.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
	};
	VkPhysicalDeviceSurfaceInfo2KHR phys_device_surface_info
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		.surface = vk_surface,
	};
	if (vkGetPhysicalDeviceSurfaceCapabilities2KHR(vk_physical_device, &phys_device_surface_info, &surface_capabilities) != VK_SUCCESS)
		return error("cannot get physical device surface capabilities2");

	u32 request_image_count = std::max(2u, surface_capabilities.surfaceCapabilities.minImageCount);
	if (surface_capabilities.surfaceCapabilities.maxImageCount > 0)
		request_image_count = std::min(request_image_count, surface_capabilities.surfaceCapabilities.maxImageCount);

	VkSwapchainCreateInfoKHR swapchain_info
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = vk_surface,
		.minImageCount = request_image_count,
		.imageFormat = SWAPCHAIN_FORMAT,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.imageExtent {.width = swapchain_width, .height = swapchain_height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = surface_capabilities.surfaceCapabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
	};

	if (vkCreateSwapchainKHR(vk_device, &swapchain_info, nullptr, &vk_swapchain) != VK_SUCCESS)
		return error("cannot create swapchain");

	u32 image_count = 0;
	vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, nullptr);
	swapchian_imgs.resize(image_count);
	swapchian_imgs_view.resize(image_count);
	if (vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, swapchian_imgs.data()) != VK_SUCCESS)
		return error("cannot get swapchain images");

	for (u32 i = 0; i < swapchian_imgs.size(); ++i)
	{
		VkImageViewCreateInfo img_view_info
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchian_imgs[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = SWAPCHAIN_FORMAT,
			.subresourceRange 
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		if (vkCreateImageView(vk_device, &img_view_info, nullptr, &swapchian_imgs_view[i]) != VK_SUCCESS)
			return error("cannot create image view");
	}

	render_completes_semaphore.resize(swapchian_imgs.size());
	for (VkSemaphore& semaphore :render_completes_semaphore)
	{
		VkSemaphoreCreateInfo semaphore_info {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		if (vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS)
			return error("cannot create render semaphore");
	}

	VkImageCreateInfo depth_info
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = DEPTH_FORMAT,
		.extent {.width = swapchain_width, .height = swapchain_height, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	}; 
	VmaAllocationCreateInfo alloc_info
	{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};
	if (vmaCreateImage(vma_allocator, &depth_info, &alloc_info, &depth_img, &depth_img_allocation, nullptr) != VK_SUCCESS)
		return error("cannot create depth image");
	VkImageViewCreateInfo depth_img_view_info
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depth_img,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = DEPTH_FORMAT,
		.subresourceRange {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1},
	};
	if (vkCreateImageView(vk_device, &depth_img_view_info, nullptr, &depth_img_view) != VK_SUCCESS)
		return error("cannot create depth view image");
	return true;
}

bool App::createShaderModule(const std::string& file_name, shaderc_shader_kind kind, VkShaderModule& shader_module)
{
	const std::string shader_path = "shader/" + file_name;
	std::string content;
	read_file(shader_path, content);
	if (content.empty())
		return error(("error with " + file_name + " shader file").c_str());

	const shaderc::Compiler compiler;
	shaderc::CompileOptions opts;
	opts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
	opts.SetTargetSpirv(shaderc_spirv_version_1_6);
	opts.SetOptimizationLevel(shaderc_optimization_level_performance);
	const shaderc::SpvCompilationResult res = compiler.CompileGlslToSpv(content, kind, file_name.c_str(), opts);

	if (res.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		std::cerr << "cannot compile shader " + file_name + ": " + res.GetErrorMessage() + "\n";
		return false;
	}
	const u32 shader_size = (res.cend() - res.cbegin()) * sizeof(u32);
	VkShaderModuleCreateInfo shader_info
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = shader_size,
		.pCode = res.cbegin(),
	};

	if (vkCreateShaderModule(vk_device, &shader_info, nullptr, &shader_module) != VK_SUCCESS)
		return error(("cannot create shader module " + file_name).c_str());

	return true;
}

bool App::createShader()
{
	if (!createShaderModule("shader.vert", shaderc_vertex_shader, vertex_shader))
		return false;
	if (!createShaderModule("shader.frag", shaderc_fragment_shader, fragment_shader))
		return false;
	return true;
}

bool App::createGraphicsPipelines()
{
	VkPipelineLayoutCreateInfo pipe_layout_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pushConstantRangeCount = 0,
	};
	if (vkCreatePipelineLayout(vk_device, &pipe_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS)
		return error("cannot create pipeline layout");

	const char* entry_point = "main";
	std::vector<VkPipelineShaderStageCreateInfo> shader_stage
	{
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = entry_point,
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = entry_point,
		}
	};
	const u32 shader_stage_count = shader_stage.size();
	VkPipelineVertexInputStateCreateInfo vertex_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};
	VkPipelineInputAssemblyStateCreateInfo assembly_state_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.stencilTestEnable = VK_FALSE,
	};
	VkPipelineViewportStateCreateInfo viewport_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
	};
	VkPipelineRasterizationStateCreateInfo raster_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};
	VkPipelineMultisampleStateCreateInfo multisample_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};
	VkPipelineColorBlendAttachmentState attach_state
	{
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT  
	};
	VkPipelineColorBlendStateCreateInfo blend_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &attach_state,
	};
	
	std::vector<VkDynamicState> dynamic_states
	{
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
	};
	const u32 dynamic_states_count = dynamic_states.size();
	VkPipelineDynamicStateCreateInfo dynamic_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = dynamic_states_count,
		.pDynamicStates = dynamic_states.data(),
	};
	VkPipelineRenderingCreateInfo render_info
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &SWAPCHAIN_FORMAT,
		.depthAttachmentFormat = DEPTH_FORMAT,
	};

	VkGraphicsPipelineCreateInfo pipeline_info
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &render_info,
		.stageCount = shader_stage_count,
		.pStages = shader_stage.data(),
		.pVertexInputState = &vertex_info,
		.pInputAssemblyState = &assembly_state_info,
		.pTessellationState = VK_NULL_HANDLE,
		.pViewportState = &viewport_info,
		.pRasterizationState = &raster_info,
		.pMultisampleState = &multisample_info,
		.pDepthStencilState = &depth_stencil_info,
		.pColorBlendState = &blend_info,
		.pDynamicState = &dynamic_info,
		.layout = pipeline_layout,
		.renderPass = VK_NULL_HANDLE,
	};
	std::cout << render_info.colorAttachmentCount << "\n";
	if (vkCreateGraphicsPipelines(vk_device, nullptr, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS)
		return error("cannot create graphics pipline");
	return true;
}

bool App::createTimeline()
{
	VkSemaphoreTypeCreateInfo semaphore_type
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = MAX_FRAME_FLIGHT,
	};
	VkSemaphoreCreateInfo semaphore_info
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &semaphore_type,
	};
	if (vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &timeline_semaphore) != VK_SUCCESS)
		return error("cannot create timeline semaphore");
	
	for (FrameRessources &res : frame_ressources)
	{
		VkSemaphoreCreateInfo semaphore_info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		if (vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &res.img_semaphore) != VK_SUCCESS)
			return error("cannot create semaphore for timeline frame");
	}

	return true;
}

bool App::createCommandBuffer()
{
	for (FrameRessources &res : frame_ressources)
	{	
		VkCommandPoolCreateInfo pool_info
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = queue_fam_idx,
		};
		if (vkCreateCommandPool(vk_device, &pool_info, nullptr, &res.cmd_pool) != VK_SUCCESS)
			return error("cannot create command pool");

		VkCommandBufferAllocateInfo cmd_buffer_info
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = res.cmd_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		if (vkAllocateCommandBuffers(vk_device, &cmd_buffer_info, &res.cmd_buffer) != VK_SUCCESS)
			return error("cannot create command buffer");
	}

	return true;
}

bool App::initialiseVulkan()
{
	if (!createVulkanInstance())
		return false;

	if (!SDL_Vulkan_CreateSurface(window, vk_instance, nullptr, &vk_surface))
		return error("sdl cannot create vulkan surface");

	if (!getPhysicalDevice())
		return false;

	if (!getGraphicsQueue())
		return false;

	if (!createDevice())
		return false;

	if (!initialiseVMA())
		return false;

	if (!createSwapchain(width, height))
		return false;

	if (!createShader())
		return false;

	if (!createGraphicsPipelines())
		return false;

	if (!createTimeline())
		return false;

	if (!createCommandBuffer())
		return false;

	return true;
}

void App::destroySwapchain()
{
	if (depth_img_view)
		vkDestroyImageView(vk_device, depth_img_view, nullptr);
	if (depth_img)
	{
		vmaDestroyImage(vma_allocator, depth_img, depth_img_allocation);
		depth_img = nullptr;
	}
	
	for (VkSemaphore &semaphore : render_completes_semaphore)
		vkDestroySemaphore(vk_device, semaphore, nullptr);
	render_completes_semaphore.clear();
	
	for (VkImageView &img_view : swapchian_imgs_view)
		vkDestroyImageView(vk_device, img_view, nullptr);
	swapchian_imgs_view.clear();

	if (vk_swapchain)
	{
		vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
		vk_swapchain = nullptr;
	}
}

bool App::initialise()
{
	if (!SDL_InitSubSystem(SDL_INIT_FLAG))
		return error("cannot init sdl");
	window = SDL_CreateWindow("my app :3", width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!window)
		return error("cant create window");

	if (!initialiseVulkan())
		return false;

	return true;
}

void App::run()
{
	bool running = true;
	while (running)
	{
		SDL_Event event {0};
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
				return ;
			else if (event.type == SDL_EVENT_WINDOW_RESIZED)
			{
				width = event.window.data1;
				height = event.window.data2;
				break; // not necessary ?
			}
		}

		render();
	}
}

void App::shutdown()
{
	SDL_SetHint(SDL_HINT_SHUTDOWN_DBUS_ON_QUIT, "1");
	//TODO FIX LEAK SDL ?????

	vkDeviceWaitIdle(vk_device);

	for (FrameRessources &res : frame_ressources)
	{
		if (res.img_semaphore)
			vkDestroySemaphore(vk_device, res.img_semaphore, nullptr);
		if (res.cmd_pool)
			vkDestroyCommandPool(vk_device, res.cmd_pool, nullptr);
	}
	vkDestroySemaphore(vk_device, timeline_semaphore, nullptr);
	
	vkDestroyPipeline(vk_device, pipeline, nullptr);
	vkDestroyPipelineLayout(vk_device, pipeline_layout, nullptr);
	
	vkDestroyShaderModule(vk_device, vertex_shader, nullptr);
	vkDestroyShaderModule(vk_device, fragment_shader, nullptr);

	destroySwapchain();

	if (vma_allocator)
		vmaDestroyAllocator(vma_allocator);

	if (vk_surface)
	{
		SDL_Vulkan_DestroySurface(vk_instance, vk_surface, nullptr);
		//vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
		//TODO same ??
	}

	if (vk_device)
		vkDestroyDevice(vk_device, nullptr);

	if (vk_instance)
		vkDestroyInstance(vk_instance, nullptr);
	volkFinalize();

	if (window)
		SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_FLAG);
	SDL_Quit();
}

void App::render(void)
{
	if (swapchain_recreate)
	{
		vkDeviceWaitIdle(vk_device);//to protect ??
		destroySwapchain();
		createSwapchain(width, height);//to protect ??
		swapchain_recreate = false;
	}

	const u64 frame_res_index = frame_index % MAX_FRAME_FLIGHT;
	const u64 signal_value = ++next_signal_value;
	const u64 wait_value = signal_value - MAX_FRAME_FLIGHT;
	++frame_index;

	VkSemaphoreWaitInfo waitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &timeline_semaphore,
		.pValues = &wait_value,
	};
	vkWaitSemaphores(vk_device, &waitInfo, UINT64_MAX);

	FrameRessources &res = frame_ressources[frame_res_index];
	vkResetCommandPool(vk_device, res.cmd_pool, 0);

	VkSemaphore image_semaphore = res.img_semaphore;
	
	//TODO see to upgrade
	//VkAcquireNextImageInfoKHR aquire_info
	//{
	//	.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
	//	.swapchain = vk_swapchain,
	//	.timeout = UINT64_MAX,
	//	.semaphore = image_semaphore,
	//	.fence = VK_NULL_HANDLE,
	//	.deviceMask = 0,
	//};
	//VkResult acquire_result = vkAcquireNextImage2KHR(vk_device, &aquire_info, &image_index);
	u32 image_index = 0;
	VkResult acquire_result = vkAcquireNextImageKHR(vk_device, vk_swapchain, UINT64_MAX, image_semaphore, VK_NULL_HANDLE, &image_index);

	if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		swapchain_recreate = true;
		return;
	}
	else if (acquire_result == VK_SUBOPTIMAL_KHR)
		swapchain_recreate = true;

	VkCommandBufferBeginInfo cmd_buffer_info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(res.cmd_buffer, &cmd_buffer_info);//to verif

	std::vector<VkImageMemoryBarrier2> layout_barriers
	{
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.image = swapchian_imgs[image_index],
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		},
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, // both specified to control memory access at both stages (write)
			.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.image = depth_img,
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		}
	};

	VkDependencyInfo depth_info{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = (u32)layout_barriers.size(),
		.pImageMemoryBarriers = layout_barriers.data(),
	};
	vkCmdPipelineBarrier2(res.cmd_buffer, &depth_info);

	VkRenderingAttachmentInfo color_attach_info
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = swapchian_imgs_view[image_index],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue {.color{0.0f, 0.0f, 0.0f, 1.0f}}
	};
	VkRenderingAttachmentInfo depth_attach_info
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depth_img_view,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue {.depthStencil{1.0f, 0}}
	};
	VkRenderingInfo render_info
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea
		{
			.offset {.x = 0, .y = 0},
			.extent {.width = swapchain_width, .height = swapchain_height}
		},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attach_info,
		.pDepthAttachment = &depth_attach_info
	};
	

	vkCmdBeginRendering(res.cmd_buffer, &render_info);

	VkViewport viewport
	{
		.x = 0, .y = 0,
		.width = (f32)swapchain_width,
		.height = (f32)swapchain_height
	};
	vkCmdSetViewport(res.cmd_buffer, 0, 1, &viewport);

	VkRect2D scissor
	{
		.offset {.x = 0, .y = 0 },
		.extent {.width = swapchain_width, .height = swapchain_height}
	};
	vkCmdSetScissor(res.cmd_buffer, 0, 1, &scissor);

	vkCmdBindPipeline(res.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdDraw(res.cmd_buffer, 3, 1, 0, 0);

	vkCmdEndRendering(res.cmd_buffer);


	VkImageMemoryBarrier2 present_layout_barrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
		.dstAccessMask = 0,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.image = swapchian_imgs[image_index],
		.subresourceRange
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
	VkDependencyInfo present_dep_barrier
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &present_layout_barrier
	};
	vkCmdPipelineBarrier2(res.cmd_buffer, &present_dep_barrier);

	vkEndCommandBuffer(res.cmd_buffer);//to verif

	VkSemaphoreSubmitInfo img_acquire_wait_info
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = image_semaphore,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	std::vector<VkSemaphoreSubmitInfo> semaphore_signals
	{
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = render_completes_semaphore[image_index],
			.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
		},
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = timeline_semaphore,
			.value = signal_value,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
		}
	};
	VkCommandBufferSubmitInfo cmd_submit_info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = res.cmd_buffer,
	};
	VkSubmitInfo2 submit_info
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &img_acquire_wait_info,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &cmd_submit_info,
		.signalSemaphoreInfoCount = (u32)semaphore_signals.size(),
		.pSignalSemaphoreInfos = semaphore_signals.data()
	};
	vkQueueSubmit2(vk_queue, 1, &submit_info, VK_NULL_HANDLE);//to verif

	VkPresentInfoKHR present_info
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &render_completes_semaphore[image_index],
		.swapchainCount = 1,
		.pSwapchains = &vk_swapchain,
		.pImageIndices = &image_index,
		.pResults = nullptr,
	};

	vkQueuePresentKHR(vk_queue, &present_info);//to verif
}
