#ifndef APP_HPP
# define APP_HPP

#include <SDL3/SDL.h>

#include <volk/volk.h>
#include <vulkan/vulkan_core.h>
#include <vma/vk_mem_alloc.h>
#include <shaderc/shaderc.h>

#include <string>
#include <vector>
#include <array>

#include "type.hpp"

struct FrameRessources
{
	VkCommandPool cmd_pool = nullptr;
	VkCommandBuffer cmd_buffer = nullptr;
	VkSemaphore img_semaphore = nullptr;
};

class App
{
	constexpr static SDL_InitFlags SDL_INIT_FLAG = SDL_INIT_VIDEO;
	SDL_Window* window = nullptr;
	i32 width = 1920;
	i32 height = 1080;

	VkInstance	vk_instance = nullptr;
	VkSurfaceKHR vk_surface = nullptr;

	VkPhysicalDevice vk_physical_device = nullptr;
	VkDevice vk_device = nullptr;
	
	u32 queue_fam_idx = UINT32_MAX;
	VkQueue vk_queue = nullptr;

	VmaAllocator vma_allocator = nullptr;

	constexpr static VkFormat SWAPCHAIN_FORMAT = VK_FORMAT_B8G8R8A8_SRGB;
	u32 swapchain_width = 0;
	u32 swapchain_height = 0;
	VkSwapchainKHR vk_swapchain = nullptr;
	std::vector<VkImage> swapchian_imgs;
	std::vector<VkImageView> swapchian_imgs_view;
	std::vector<VkSemaphore> render_completes_semaphore;

	constexpr static VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
	VkImage depth_img = nullptr;
	VkImageView depth_img_view = nullptr;
	VmaAllocation depth_img_allocation = nullptr;

	VkShaderModule vertex_shader = nullptr;
	VkShaderModule fragment_shader = nullptr;

	VkPipelineLayout pipeline_layout = nullptr;
	VkPipeline pipeline = nullptr;

	constexpr static u32 MAX_FRAME_FLIGHT = 2;
	VkSemaphore timeline_semaphore = nullptr;
	std::array<FrameRessources, MAX_FRAME_FLIGHT> frame_ressources;

	void render();
	bool createVulkanInstance();
	bool initialiseVulkan();
	bool getPhysicalDevice();
	bool getGraphicsQueue();
	bool createDevice();
	bool initialiseVMA();
	bool createSwapchain(u32 width, u32 height);
	bool createShaderModule(const std::string& name, shaderc_shader_kind kind, VkShaderModule& shader_module);
	bool createShader();
	bool createGraphicsPipelines();
	bool createRessources();

	void destroySwapchain();


public:
	constexpr static u32 VK_VERSION = VK_API_VERSION_1_4;
	bool initialise();
	void run();
	void shutdown();
};

void read_file(const std::string& path, std::string& res);

#endif
