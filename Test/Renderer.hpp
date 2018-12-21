#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <memory>
#include "Outline.hpp"

struct CellInfo
{
	uint32_t point_offset;
	uint32_t cell_offset;
	uint32_t cell_count_x;
	uint32_t cell_count_y;
};

struct GlyphInstance
{
	acid::Rect rect;
	uint32_t glyph_index;
	float sharpness;
};

struct HostGlyphInfo
{
	acid::Rect bbox;
	float advance;
};

struct DeviceGlyphInfo
{
	acid::Rect bbox;
	CellInfo cell_info;
};

class Renderer
{
private:
	uint32_t m_frame;
	uint32_t m_fps_temp;
	uint32_t m_fps;
	float m_time;
	float m_delta_time;
	uint32_t m_ring_buffer_index;
	uint32_t m_ring_buffer_count;

	uint32_t m_width;
	uint32_t m_height;

	acid::Vector2 m_mouse_pos;
	acid::Vector2 m_old_mouse_pos;
	acid::Vector2 m_canvas_offset;
	float m_canvas_scale;
	float m_target_canvas_scale;

	acid::Outline *m_outlines;
	HostGlyphInfo *m_glyph_infos;

	void *m_glyph_data;
	uint32_t m_glyph_data_size;
	uint32_t m_glyph_info_size;
	uint32_t m_glyph_cells_size;
	uint32_t m_glyph_points_size;
	uint32_t m_glyph_info_offset;
	uint32_t m_glyph_cells_offset;
	uint32_t m_glyph_points_offset;

	GlyphInstance *m_glyph_instances;
	uint32_t m_glyph_instance_count;

	GLFWwindow *m_window;
	VkInstance m_instance;
	VkPhysicalDevice m_physical_device;
	VkPhysicalDeviceMemoryProperties m_memory_properties;
	VkPhysicalDeviceProperties m_device_properties;
	uint32_t m_graphics_queue_family;
	uint32_t m_present_queue_family;
	VkDevice m_device;
	VkQueue m_graphics_queue;
	VkQueue m_present_queue;
	VkSurfaceKHR m_surface;
	VkFormat m_format;
	VkColorSpaceKHR m_color_space;
	VkPresentModeKHR m_present_mode;
	VkSwapchainKHR m_swapchain;
	VkExtent2D m_swapchain_extent;
	uint32_t m_image_index;
	uint32_t m_image_count;
	std::unique_ptr<VkImage[]> m_images;
	std::unique_ptr<VkImageView[]> m_image_views;
	VkRenderPass m_render_pass;
	std::unique_ptr<VkFramebuffer[]> m_framebuffers;
	VkSemaphore m_image_available_semaphore;
	VkSemaphore m_render_finished_semaphore;

	VkDeviceMemory m_storage_buffer_memory;
	VkBuffer m_storage_buffer;
	VkDeviceMemory m_instance_buffer_memory;
	VkBuffer m_instance_buffer;
	VkDeviceMemory m_instance_staging_buffer_memory;
	VkBuffer m_instance_staging_buffer;
	VkCommandPool m_command_pool;
	std::unique_ptr<VkCommandBuffer[]> m_command_buffers;
	std::unique_ptr<VkFence[]> m_command_buffer_fences;
	VkDescriptorPool m_descriptor_pool;
	VkDescriptorSet m_descriptor_set;
	VkDescriptorSetLayout m_set_layout;
	VkPipelineLayout m_pipeline_layout;
	VkPipeline m_pipeline;

	friend void CallbackWindowResized(GLFWwindow* window, int width, int height);
	friend void CallbackWindowRefresh(GLFWwindow* window);
	friend void CallbackMouseButton(GLFWwindow* window, int button, int action, int mods);
	friend void CallbackKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	friend void CallbackScroll(GLFWwindow* window, double xoffset, double yoffset);
public:
	static const uint32_t MAX_VISIBLE_GLYPHS;
	static const uint32_t NUMBER_OF_GLYPHS;

	Renderer(const uint32_t &width, const uint32_t &height);

	~Renderer();

	void Run();

	static void CheckVk(const VkResult &result);

	static uint32_t align_uint32(const uint32_t &value, const uint32_t &alignment);
private:
	void Update();

	void RenderFrame();

	void load_font();
	void create_instance();
	void create_surface();
	void pick_physical_device();
	void create_device();
	void find_queue_families();
	void choose_surface_format();
	void choose_present_mode();
	void create_swap_chain();
	void create_image_views();
	void create_render_pass();
	void create_framebuffers();
	void create_command_pool();
	void begin_text();
	void end_text();
	void append_text(float x, float y, float scale, const std::string &text);
	void record_current_command_buffer();
	void record_command_buffers();
	void create_command_buffers();
	void create_command_buffer_fences();
	void create_semaphores();
	VkShaderModule load_shader_module(VkDevice device, const char *path);
	void create_layout();
	uint32_t find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags flags);
	VkDeviceMemory alloc_required_memory(VkMemoryRequirements *req, VkMemoryPropertyFlags flags);
	void create_buffer_with_memory(VkBufferCreateInfo *ci, VkMemoryPropertyFlags flags, VkDeviceMemory *memory, VkBuffer *buffer);
	VkCommandBuffer begin_one_time_cmdbuf();
	void end_one_time_cmdbuf(VkCommandBuffer cmd_buffer);
	void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
	void stage_buffer(VkBuffer buffer, void *data, size_t size);
	void create_storage_buffer();
	void create_instance_buffer();
	void create_descriptor_pool();
	void create_descriptor_set();
	void create_pipeline();
	void create_swap_chain_objects();
	void create_vulkan_objects();
	void destroy_swap_chain_objects();
	void destroy_vulkan_objects();
	void recreate_swap_chain();
};
