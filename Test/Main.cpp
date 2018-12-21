#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <memory>
#include "Outline.hpp"

static uint32_t WIDTH = 1280;
static uint32_t HEIGHT = 720;

static const uint32_t MAX_VISIBLE_GLYPHS = 4096;
static const uint32_t NUMBER_OF_GLYPHS = 128;

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

struct Render
{
	uint32_t frame;
	uint32_t fps_temp;
	uint32_t fps;
	float time;
	float delta_time;
	uint32_t ring_buffer_index;
	uint32_t ring_buffer_count;

	acid::Vector2 mouse_pos;
	acid::Vector2 old_mouse_pos;
	acid::Vector2 canvas_offset;
	float canvas_scale;
	float target_canvas_scale;

	acid::Outline outlines[NUMBER_OF_GLYPHS];
	HostGlyphInfo glyph_infos[NUMBER_OF_GLYPHS];

	void *glyph_data;
	uint32_t glyph_data_size;
	uint32_t glyph_info_size;
	uint32_t glyph_cells_size;
	uint32_t glyph_points_size;
	uint32_t glyph_info_offset;
	uint32_t glyph_cells_offset;
	uint32_t glyph_points_offset;

	GlyphInstance *glyph_instances;
	uint32_t glyph_instance_count;

	GLFWwindow *window;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkPhysicalDeviceProperties device_properties;
	uint32_t graphics_queue_family;
	uint32_t present_queue_family;
	VkDevice device;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkSurfaceKHR surface;
	VkFormat format;
	VkColorSpaceKHR color_space;
	VkPresentModeKHR present_mode;
	VkSwapchainKHR swapchain;
	VkExtent2D swapchain_extent;
	uint32_t image_index;
	uint32_t image_count;
	std::unique_ptr<VkImage[]> images;
	std::unique_ptr<VkImageView[]> image_views;
	VkRenderPass render_pass;
	std::unique_ptr<VkFramebuffer[]> framebuffers;
	VkSemaphore image_available_semaphore;
	VkSemaphore render_finished_semaphore;

	VkDeviceMemory storage_buffer_memory;
	VkBuffer storage_buffer;
	VkDeviceMemory instance_buffer_memory;
	VkBuffer instance_buffer;
	VkDeviceMemory instance_staging_buffer_memory;
	VkBuffer instance_staging_buffer;
	VkCommandPool command_pool;
	std::unique_ptr<VkCommandBuffer[]> command_buffers;
	std::unique_ptr<VkFence[]> command_buffer_fences;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set;
	VkDescriptorSetLayout set_layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
};

static void CheckVk(const VkResult &result)
{
	if (result >= 0)
	{
		return;
	}

	assert(false && "Vulkan error!");
}

static uint32_t align_uint32(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

static void load_font(Render *r)
{
	FT_Library library;
	FT_CHECK(FT_Init_FreeType(&library));

	FT_Face face;
	FT_CHECK(FT_New_Face(library, "Resources/Fonts/Pacifico-Regular.ttf", 0, &face));
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/Lato-Medium.ttf", 0, &face));
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/Lobster-Regular.ttf", 0, &face));
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/LobsterTwo-Italic.ttf", 0, &face));
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/OpenSans-Regular.ttf", 0, &face));

	FT_CHECK(FT_Set_Char_Size(face, 0, 1000 * 64, 96, 96));

	uint32_t total_points = 0;
	uint32_t total_cells = 0;

	/*FT_ULong charcode;
	FT_UInt gindex;
	charcode = FT_Get_First_Char(face, &gindex);

	while (gindex != 0)
	{
		acid::Outline &o = r->outlines[gindex];
		HostGlyphInfo &hgi = r->glyph_infos[gindex];

		FT_CHECK(FT_Load_Glyph(face, gindex, FT_LOAD_NO_HINTING));

		outline_convert(&face->glyph->outline, o);

		hgi.bbox = o.bbox;
		hgi.advance = face->glyph->metrics.horiAdvance / 64.0f;

		total_points += o.pointCount;
		total_cells += o.cellCountX * o.cellCountY;
		charcode = FT_Get_Next_Char(face, charcode, &gindex);
	}*/
	for (uint32_t i = 0; i < NUMBER_OF_GLYPHS; i++)
	{
		char c = ' ' + i;
		printf("%c", c);

		acid::Outline *o = &r->outlines[i];
		HostGlyphInfo *hgi = &r->glyph_infos[i];

		FT_UInt glyph_index = FT_Get_Char_Index(face, c);
		FT_CHECK(FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_HINTING));

		o->outline_convert(&face->glyph->outline);

		hgi->bbox = o->bbox;
		hgi->advance = face->glyph->metrics.horiAdvance / 64.0f;

		total_points += o->pointCount;
		total_cells += o->cellCountX * o->cellCountY;
	}

	r->glyph_info_size = sizeof(DeviceGlyphInfo) * NUMBER_OF_GLYPHS;
	r->glyph_cells_size = sizeof(uint32_t) * total_cells;
	r->glyph_points_size = sizeof(acid::Vector2) * total_points;

	uint32_t alignment = r->device_properties.limits.minStorageBufferOffsetAlignment;
	r->glyph_info_offset = 0;
	r->glyph_cells_offset = align_uint32(r->glyph_info_size, alignment);
	r->glyph_points_offset = align_uint32(r->glyph_info_size + r->glyph_cells_size, alignment);
	r->glyph_data_size = r->glyph_points_offset + r->glyph_points_size;

	r->glyph_data = malloc(r->glyph_data_size);

	DeviceGlyphInfo *device_glyph_infos = (DeviceGlyphInfo*)((char*)r->glyph_data + r->glyph_info_offset);
	uint32_t *cells = (uint32_t*)((char*)r->glyph_data + r->glyph_cells_offset);
	acid::Vector2 *points = (acid::Vector2*)((char*)r->glyph_data + r->glyph_points_offset);

	uint32_t point_offset = 0;
	uint32_t cell_offset = 0;

	for (uint32_t i = 0; i < NUMBER_OF_GLYPHS; i++)
	{
		acid::Outline *o = &r->outlines[i];
		DeviceGlyphInfo *dgi = &device_glyph_infos[i];

		dgi->cell_info.cell_count_x = o->cellCountX;
		dgi->cell_info.cell_count_y = o->cellCountY;
		dgi->cell_info.point_offset = point_offset;
		dgi->cell_info.cell_offset = cell_offset;
		dgi->bbox = o->bbox;

		uint32_t cell_count = o->cellCountX * o->cellCountY;
		memcpy(cells + cell_offset, o->cells, sizeof(uint32_t) * cell_count);
		memcpy(points + point_offset, o->points, sizeof(acid::Vector2) * o->pointCount);

		point_offset += o->pointCount;
		cell_offset += cell_count;
	}

	assert(point_offset == total_points);
	assert(cell_offset == total_cells);

	for (auto &outline : r->outlines)
	{
		outline.outline_destroy();
	}

	FT_CHECK(FT_Done_Face(face));
	FT_CHECK(FT_Done_FreeType(library));
}

static void create_instance(Render *r)
{
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "Font rendering demo",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_1,
	};

	uint32_t requiredInstanceExtensionCount;
	auto requiredInstanceExtensions = glfwGetRequiredInstanceExtensions(&requiredInstanceExtensionCount);

	VkInstanceCreateInfo instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = requiredInstanceExtensionCount,
		.ppEnabledExtensionNames = requiredInstanceExtensions,
	};

	CheckVk(vkCreateInstance(&instance_info, nullptr, &r->instance));
}

static void create_surface(Render *r)
{
	CheckVk(glfwCreateWindowSurface(r->instance, r->window, nullptr, &r->surface));
}

static void pick_physical_device(Render *r)
{
	uint32_t physical_device_count;
	CheckVk(vkEnumeratePhysicalDevices(r->instance, &physical_device_count, nullptr));
	assert(physical_device_count > 0);

	std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
	CheckVk(vkEnumeratePhysicalDevices(r->instance, &physical_device_count, physical_devices.data()));

	r->physical_device = physical_devices[0];

	vkGetPhysicalDeviceMemoryProperties(r->physical_device, &r->memory_properties);
	vkGetPhysicalDeviceProperties(r->physical_device, &r->device_properties);
}

static void create_device(Render *r)
{
	char const * extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = nullptr,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = extensions,
	};

	float queue_priority = 1.0f;

	if (r->graphics_queue_family == r->present_queue_family)
	{
		VkDeviceQueueCreateInfo queue_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.queueFamilyIndex = r->graphics_queue_family,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority,
		};

		device_info.pQueueCreateInfos = &queue_info;
		device_info.queueCreateInfoCount = 1;

		CheckVk(vkCreateDevice(r->physical_device, &device_info, nullptr, &r->device));
	}
	else
	{
		VkDeviceQueueCreateInfo queue_infos[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = nullptr,
				.queueFamilyIndex = r->graphics_queue_family,
				.queueCount = 1,
				.pQueuePriorities = &queue_priority,
			},
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = nullptr,
				.queueFamilyIndex = r->present_queue_family,
				.queueCount = 1,
				.pQueuePriorities = &queue_priority,
			},
		};

		device_info.pQueueCreateInfos = queue_infos;
		device_info.queueCreateInfoCount = 2;

		CheckVk(vkCreateDevice(r->physical_device, &device_info, nullptr, &r->device));
	}

	vkGetDeviceQueue(r->device, r->graphics_queue_family, 0, &r->graphics_queue);
	vkGetDeviceQueue(r->device, r->present_queue_family, 0, &r->present_queue);
}

static void find_queue_families(Render *r)
{
	uint32_t family_count;

	vkGetPhysicalDeviceQueueFamilyProperties(r->physical_device, &family_count, nullptr);

	std::vector<VkQueueFamilyProperties> properties(family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(r->physical_device, &family_count, properties.data());

	r->graphics_queue_family = UINT32_MAX;
	r->present_queue_family = UINT32_MAX;

	for (uint32_t i = 0; i < family_count; i++)
	{
		if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			VkBool32 present;
			CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(r->physical_device, i, r->surface, &present));

			if (present)
			{
				r->graphics_queue_family = i;
				r->present_queue_family = i;
				break;
			}
			else
			{
				if (r->graphics_queue_family == UINT32_MAX)
					r->graphics_queue_family = i;
			}
		}
	}

	if (r->present_queue_family == UINT32_MAX)
	{
		for (uint32_t i = 0; i < family_count; i++)
		{
			VkBool32 present;
			CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(r->physical_device, i, r->surface, &present));

			if (present)
			{
				r->present_queue_family = i;
				break;
			}
		}
	}
}

static void choose_surface_format(Render *r)
{
	uint32_t format_count;
	CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(
		r->physical_device, r->surface, &format_count, nullptr));

	assert(format_count > 0);

	std::vector<VkSurfaceFormatKHR> formats(format_count);
	CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(
		r->physical_device, r->surface, &format_count, formats.data()));
	/*
	if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		r->format = VK_FORMAT_B8G8R8A8_UNORM;
	else r->format = formats[0].format;
	*/
	r->format = VK_FORMAT_B8G8R8A8_SRGB;
}

static void choose_present_mode(Render *r)
{
	uint32_t present_mode_count;
	CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(
		r->physical_device, r->surface, &present_mode_count, nullptr));

	assert(present_mode_count > 0);

	std::vector<VkPresentModeKHR>	present_modes(present_mode_count);
	CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(
		r->physical_device, r->surface, &present_mode_count, present_modes.data()));


	for (uint32_t i = 0; i < present_mode_count; i++)
	{
		if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			r->present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			return;
		}
	}

	//r->present_mode = VK_PRESENT_MODE_FIFO_KHR;
	r->present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
}

static void create_swap_chain(Render *r)
{
	VkSurfaceCapabilitiesKHR capabilities;
	CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r->physical_device, r->surface, &capabilities));

	uint32_t min_image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount && min_image_count > capabilities.maxImageCount)
		min_image_count = capabilities.maxImageCount;

	choose_surface_format(r);
	choose_present_mode(r);

	VkSwapchainKHR old_swapchain = r->swapchain;
	r->swapchain = nullptr;

	VkSwapchainCreateInfoKHR swapchain_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = r->surface,
		.minImageCount = min_image_count,
		.imageFormat = r->format,
		.imageColorSpace = r->color_space,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = r->present_mode,
		.clipped = VK_TRUE,
		.oldSwapchain = old_swapchain,
	};

	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		swapchain_info.imageExtent = capabilities.currentExtent;
	}
	else
	{
		int window_w, window_h;
		glfwGetWindowSize(r->window, &window_w, &window_h);

		uint32_t w, h;
		w = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, (uint32_t)window_w));
		h = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, (uint32_t)window_h));

		swapchain_info.imageExtent.width = w;
		swapchain_info.imageExtent.height = h;
	}

	r->swapchain_extent = swapchain_info.imageExtent;

	if (r->graphics_queue_family != r->present_queue_family)
	{
		uint32_t families[] = { r->graphics_queue_family, r->present_queue_family };
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_info.queueFamilyIndexCount = 2;
		swapchain_info.pQueueFamilyIndices = families;
	}
	else
	{
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_info.queueFamilyIndexCount = 0;
		swapchain_info.pQueueFamilyIndices = nullptr;
	}

	CheckVk(vkCreateSwapchainKHR(r->device, &swapchain_info, nullptr, &r->swapchain));

	if (old_swapchain)
		vkDestroySwapchainKHR(r->device, old_swapchain, nullptr);

	CheckVk(vkGetSwapchainImagesKHR(r->device, r->swapchain, &r->image_count, nullptr));

	r->images = std::make_unique<VkImage[]>(r->image_count);
	CheckVk(vkGetSwapchainImagesKHR(r->device, r->swapchain, &r->image_count, r->images.get()));
}

static void create_image_views(Render *r)
{
	r->image_views = std::make_unique<VkImageView[]>(r->image_count);
	memset(r->image_views.get(), 0, sizeof(VkImageView) * r->image_count);

	for (uint32_t i = 0; i < r->image_count; i++)
	{
		VkImageViewCreateInfo ci = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = r->images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = r->format,
			.components = { },
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		CheckVk(vkCreateImageView(r->device, &ci, nullptr, &r->image_views[i]));
	}
}

static void create_render_pass(Render *r)
{
	VkAttachmentDescription attachments[] = {
		{
			.format = r->format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		},
	};

	VkAttachmentReference color_attachment_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_ref,
	};

	VkSubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
	};

	VkRenderPassCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &dependency,
	};

	CheckVk(vkCreateRenderPass(r->device, &ci, nullptr, &r->render_pass));
}

static void create_framebuffers(Render *r)
{
	r->framebuffers = std::make_unique<VkFramebuffer[]>(r->image_count);
	memset(r->framebuffers.get(), 0, sizeof(VkFramebuffer) * r->image_count);

	for (uint32_t i = 0; i < r->image_count; i++)
	{
		VkImageView attachments[] = {
			r->image_views[i],
		};

		VkFramebufferCreateInfo ci = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = r->render_pass,
			.attachmentCount = 1,
			.pAttachments = attachments,
			.width = r->swapchain_extent.width,
			.height = r->swapchain_extent.height,
			.layers = 1,
		};

		CheckVk(vkCreateFramebuffer(r->device, &ci, nullptr, &r->framebuffers[i]));
	}
}

static void create_command_pool(Render *r)
{
	VkCommandPoolCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = r->graphics_queue_family,
	};

	CheckVk(vkCreateCommandPool(r->device, &ci, nullptr, &r->command_pool));
}


static void begin_text(Render *r)
{
	r->glyph_instance_count = 0;

	uint32_t size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance);
	uint32_t offset = size * r->ring_buffer_index;

	CheckVk(vkMapMemory(r->device, r->instance_staging_buffer_memory, offset, size, 0, (void **)&r->glyph_instances));
}

static void end_text(Render *r)
{
	VkCommandBuffer cmd_buf = r->command_buffers[r->ring_buffer_index];
	uint32_t size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance);
	uint32_t offset = size * r->ring_buffer_index;

	vkUnmapMemory(r->device, r->instance_staging_buffer_memory);

	VkBufferMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.buffer = r->instance_buffer,
		.offset = 0,
		.size = size,
	};

	vkCmdPipelineBarrier(
		cmd_buf,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr);

	VkBufferCopy copy = {
		.srcOffset = offset,
		.dstOffset = 0,
		.size = size
	};

	vkCmdCopyBuffer(cmd_buf, r->instance_staging_buffer, r->instance_buffer, 1, &copy);

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

	vkCmdPipelineBarrier(
		cmd_buf,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		0,
		0, nullptr,
		1, &barrier,
		0, nullptr);
}

static void append_text(Render *r, float x, float y, float scale, const std::string &text)
{
	for (char c : text)
	{
		if (r->glyph_instance_count >= MAX_VISIBLE_GLYPHS)
			break;

		uint32_t glyph_index = static_cast<uint32_t>(c) - 32;

		HostGlyphInfo *gi = &r->glyph_infos[glyph_index];
		GlyphInstance *inst = &r->glyph_instances[r->glyph_instance_count];

		inst->rect.minX = (x + gi->bbox.minX * scale) / (r->swapchain_extent.width / 2.0f) - 1.0f;
		inst->rect.minY = (y - gi->bbox.minY * scale) / (r->swapchain_extent.height / 2.0f) - 1.0f;
		inst->rect.maxX = (x + gi->bbox.maxX * scale) / (r->swapchain_extent.width / 2.0f) - 1.0f;
		inst->rect.maxY = (y - gi->bbox.maxY * scale) / (r->swapchain_extent.height / 2.0f) - 1.0f;

		if (inst->rect.minX <= 1 && inst->rect.maxX >= -1 &&
			inst->rect.maxY <= 1 && inst->rect.minY >= -1)
		{
			inst->glyph_index = glyph_index;
			inst->sharpness = scale;

			r->glyph_instance_count++;
		}

		x += gi->advance * scale;
	}
}

static void record_current_command_buffer(Render *r)
{
	//printf("%d\n", r->ring_buffer_index);
	VkCommandBuffer cmd_buf = r->command_buffers[r->ring_buffer_index];

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
	};

	CheckVk(vkBeginCommandBuffer(cmd_buf, &begin_info));

	begin_text(r);

	append_text(r, 5.0f, HEIGHT - 10.0f, 0.02f, "Frame time: " + std::to_string((int32_t)(1000.0f / r->fps)) + " ms");
	append_text(r, 5.0f, HEIGHT - 40.0f, 0.02f, "FPS: " + std::to_string(r->fps));

	std::vector<std::string> lines = {
	//	"\\u0000ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890^$€*",
		"@&(3 Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus sit amet scelerisque augue, sit amet commodo neque. Vestibulum",
		"eu eros a justo molestie bibendum quis in urna. Integer quis tristique magna. Morbi in ultricies lorem. Donec lacinia nisi et",
		"arcu scelerisque, eget viverra ante dapibus. Proin enim neque, vehicula id congue quis, consequat sit amet tortor.Aenean ac",
		"lorem sit amet magna rhoncus rhoncus ac ac neque. Cras sed rutrum sem. Donec placerat ultricies ex, a gravida lorem commodo ut.",
		"Mauris faucibus aliquet ligula, vitae condimentum dui semper et. Aenean pellentesque ac ligula a varius. Suspendisse congue",
		"lorem lorem, ac consectetur ipsum condimentum id.",
		"",
		"Vestibulum quis erat sem. Fusce efficitur libero et leo sagittis, ac volutpat felis ullamcorper. Curabitur fringilla eros eget ex",
		"lobortis, at posuere sem consectetur. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis",
		"egestas. Vivamus eu enim leo. Morbi ultricies lorem et pellentesque vestibulum. Proin eu ultricies sem. Quisque laoreet, ligula",
		"non molestie congue, odio nunc tempus arcu, vel aliquet leo turpis non enim. Sed risus dui, condimentum et eros a, molestie",
		"imperdiet nisl. Vivamus quis ante venenatis, cursus magna ut, tincidunt elit. Aenean nisl risus, porttitor et viverra quis,",
		"tempus vitae nisl.",
		"",
		"Suspendisse ut scelerisque tellus. In ac quam sem.Curabitur suscipit massa nisl. Ut et metus sed lacus dapibus molestie. nullptram",
		"porttitor sit amet magna quis dapibus. nullptra tincidunt, arcu sit amet hendrerit consequat, felis leo blandit libero, eu posuere",
		"nisl quam interdum nullptra. Quisque nec efficitur libero. Quisque quis orci vitae metus feugiat aliquam eu et nullptra. Etiam aliquet",
		"ante vitae lacus aliquam, et gravida elit mollis. Proin molestie, justo tempus rhoncus aliquam, tellus erat venenatis erat,",
		"porttitor dapibus nunc purus id enim. Integer a nunc ut velit porta maximus. nullptram rutrum nisi in sagittis pharetra. Proin id",
		"pharetra augue, sed vulputate lorem. Aenean dapibus, turpis nec ullamcorper pharetra, ex augue congue nibh, condimentum",
		"vestibulum arcu urna quis ex.",
		"",
		"Vestibulum non dignissim nibh, quis vestibulum odio. Ut sed viverra ante, fringilla convallis tellus. Donec in est rutrum,",
		"imperdiet dolor a, vestibulum magna. In nec justo tellus. Ut non erat eu leo ornare imperdiet in sit amet lorem. nullptram quis",
		"nisl diam. Aliquam laoreet dui et ligula posuere cursus.",
		"",
		"Donec vestibulum ante eget arcu dapibus lobortis.Curabitur condimentum tellus felis, id luctus mi ultrices quis. Aenean nullptra",
		"justo, venenatis vel risus et, suscipit faucibus nullptra. Pellentesque habitant morbi tristique senectus et netus et malesuada",
		"fames ac turpis egestas. Sed lacinia metus eleifend lacinia blandit.Morbi est nibh, dapibus nec arcu quis, volutpat lacinia",
		"dolor. Vestibulum quis viverra erat.Maecenas ultricies odio neque, et eleifend arcu auctor in. Suspendisse placerat massa nisl,",
		"non condimentum ligula sodales at.Phasellus eros urna, elementum in ultricies quis, vulputate id magna. Donec efficitur rutrum",
		"urna sed tempus. Vestibulum eu augue dolor. Vestibulum vehicula suscipit purus, sit amet ultricies ligula malesuada sit amet.",
		"Duis consectetur elit euismod arcu aliquet vehicula. Pellentesque lobortis dui et nisl vehicula, in placerat quam dapibus.Fusce",
		"auctor arcu a purus bibendum, eget blandit nisi lobortis.",
	};

	for (uint32_t i = 0; i < lines.size(); i++)
	{
		append_text(r,
			r->canvas_scale * (10.0f - r->canvas_offset[0]),
			r->canvas_scale * (30.0f - r->canvas_offset[1] + i * 30.0f),
			0.02f * r->canvas_scale,
			lines[i]);
	}

	end_text(r);

	VkClearValue clear_value = { 1.0f, 1.0f, 1.0f, 1.0f }; // 0.1f, 0.2f, 0.3f
	VkRenderPassBeginInfo render_pass_bi = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = r->render_pass,
		.framebuffer = r->framebuffers[r->image_index],
		.renderArea = {
			.offset = { 0, 0 },
			.extent = r->swapchain_extent
		},
		.clearValueCount = 1,
		.pClearValues = &clear_value
	};

	vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd_buf, 0, 1, &r->instance_buffer, offsets);

	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
		r->pipeline_layout, 0, 1, &r->descriptor_set, 0, nullptr);

	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipeline);
	vkCmdDraw(cmd_buf, 4, r->glyph_instance_count, 0, 0);

	vkCmdEndRenderPass(cmd_buf);
	CheckVk(vkEndCommandBuffer(cmd_buf));
}

static void record_command_buffers(Render *r)
{
	for (uint32_t i = 0; i < r->ring_buffer_count; i++)
	{
		r->ring_buffer_index = i;
		record_current_command_buffer(r);
	}

}

static void create_command_buffers(Render *r)
{
	r->command_buffers = std::make_unique<VkCommandBuffer[]>(r->ring_buffer_count);
	memset(r->command_buffers.get(), 0, sizeof(VkCommandBuffer) * r->ring_buffer_count);

	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = r->command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = r->ring_buffer_count,
	};

	CheckVk(vkAllocateCommandBuffers(r->device, &alloc_info, r->command_buffers.get()));
	record_command_buffers(r);
}

static void create_command_buffer_fences(Render *r)
{
	r->command_buffer_fences = std::make_unique<VkFence[]>(r->ring_buffer_count);

	VkFenceCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (uint32_t i = 0; i < r->ring_buffer_count; i++)
		CheckVk(vkCreateFence(r->device, &ci, nullptr, &r->command_buffer_fences[i]));
}

static void create_semaphores(Render *r)
{
	VkSemaphoreCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	CheckVk(vkCreateSemaphore(r->device, &ci, nullptr, &r->image_available_semaphore));
	CheckVk(vkCreateSemaphore(r->device, &ci, nullptr, &r->render_finished_semaphore));
}

static VkShaderModule load_shader_module(VkDevice device, const char *path)
{
	FILE *f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);

	auto code = std::vector<uint32_t>(size / sizeof(uint32_t));
	fread(code.data(), size, 1, f);
	fclose(f);

	VkShaderModuleCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size_t(size),
		.pCode = code.data(),
	};

	VkShaderModule ret;
	CheckVk(vkCreateShaderModule(device, &ci, nullptr, &ret));

	return ret;
}

static void create_layout(Render *r)
{
	VkDescriptorSetLayoutBinding bindings[] = {
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		},
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		},
		{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		}
	};

	VkDescriptorSetLayoutCreateInfo layout_ci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 3,
		.pBindings = bindings,
	};

	CheckVk(vkCreateDescriptorSetLayout(r->device, &layout_ci, nullptr, &r->set_layout));

	VkPipelineLayoutCreateInfo pipeline_ci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &r->set_layout,
	};

	CheckVk(vkCreatePipelineLayout(r->device, &pipeline_ci, nullptr, &r->pipeline_layout));
}

static uint32_t find_memory_type(Render *r, uint32_t type_bits, VkMemoryPropertyFlags flags)
{
	for (uint32_t i = 0; i < r->memory_properties.memoryTypeCount; i++)
	{
		if (type_bits & 1 << i)
		{
			VkMemoryPropertyFlags f = r->memory_properties.memoryTypes[i].propertyFlags;

			if ((f & flags) == flags)
				return i;
		}
	}

	return UINT32_MAX;
}

static VkDeviceMemory alloc_required_memory(Render *r, VkMemoryRequirements *req, VkMemoryPropertyFlags flags)
{
	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = req->size,
		.memoryTypeIndex = find_memory_type(r, req->memoryTypeBits, flags),
	};

	VkDeviceMemory mem;
	CheckVk(vkAllocateMemory(r->device, &alloc_info, nullptr, &mem));
	return mem;
}

static void create_buffer_with_memory(
	Render *r, VkBufferCreateInfo *ci, VkMemoryPropertyFlags flags,
	VkDeviceMemory *memory, VkBuffer *buffer)
{
	CheckVk(vkCreateBuffer(r->device, ci, nullptr, buffer));

	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(r->device, *buffer, &req);

	*memory = alloc_required_memory(r, &req, flags);
	CheckVk(vkBindBufferMemory(r->device, *buffer, *memory, 0));
}

VkCommandBuffer begin_one_time_cmdbuf(Render *r)
{
	VkCommandBufferAllocateInfo cmd_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = r->command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VkCommandBuffer cmd_buffer;
	CheckVk(vkAllocateCommandBuffers(r->device, &cmd_alloc_info, &cmd_buffer));

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	CheckVk(vkBeginCommandBuffer(cmd_buffer, &begin_info));
	return cmd_buffer;
}

void end_one_time_cmdbuf(Render *r, VkCommandBuffer cmd_buffer)
{
	CheckVk(vkEndCommandBuffer(cmd_buffer));

	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd_buffer,
	};

	CheckVk(vkQueueSubmit(r->graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
	CheckVk(vkQueueWaitIdle(r->graphics_queue));
	vkFreeCommandBuffers(r->device, r->command_pool, 1, &cmd_buffer);
}

static void copy_buffer(Render *r, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
	VkCommandBuffer cmd_buf = begin_one_time_cmdbuf(r);
	VkBufferCopy copy = { 0, 0, size };

	vkCmdCopyBuffer(cmd_buf, src_buffer, dst_buffer, 1, &copy);
	end_one_time_cmdbuf(r, cmd_buf);
}

static void stage_buffer(Render *r, VkBuffer buffer, void *data, size_t size)
{
	VkBufferCreateInfo staging_ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	create_buffer_with_memory(r, &staging_ci,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		&staging_buffer_memory, &staging_buffer);

	void *staging_buffer_ptr;
	CheckVk(vkMapMemory(r->device, staging_buffer_memory, 0, staging_ci.size, 0, &staging_buffer_ptr));

	memcpy(staging_buffer_ptr, data, size);

	vkUnmapMemory(r->device, staging_buffer_memory);

	copy_buffer(r, staging_buffer, buffer, size);

	vkDestroyBuffer(r->device, staging_buffer, nullptr);
	vkFreeMemory(r->device, staging_buffer_memory, nullptr);
}

static void create_storage_buffer(Render *r)
{
	load_font(r);

	VkBufferCreateInfo storage_ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = r->glyph_data_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	create_buffer_with_memory(r, &storage_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&r->storage_buffer_memory, &r->storage_buffer);

	stage_buffer(r, r->storage_buffer, r->glyph_data, storage_ci.size);

	free(r->glyph_data);
	r->glyph_data = nullptr;
}

static void create_instance_buffer(Render *r)
{
	VkBufferCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance),
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	create_buffer_with_memory(r, &ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&r->instance_buffer_memory, &r->instance_buffer);

	VkBufferCreateInfo staging_ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = ci.size * r->ring_buffer_count,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	create_buffer_with_memory(r, &staging_ci,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		&r->instance_staging_buffer_memory, &r->instance_staging_buffer);
}

static void create_descriptor_pool(Render *r)
{
	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
	};

	VkDescriptorPoolCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = pool_sizes,
	};

	CheckVk(vkCreateDescriptorPool(r->device, &ci, nullptr, &r->descriptor_pool));
}

static void create_descriptor_set(Render *r)
{
	VkDescriptorSetAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = r->descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &r->set_layout,
	};

	CheckVk(vkAllocateDescriptorSets(r->device, &alloc_info, &r->descriptor_set));

	VkDescriptorBufferInfo glyph_info = {
		.buffer = r->storage_buffer,
		.offset = r->glyph_info_offset,
		.range = r->glyph_info_size,
	};

	VkDescriptorBufferInfo cells_info = {
		.buffer = r->storage_buffer,
		.offset = r->glyph_cells_offset,
		.range = r->glyph_cells_size,
	};

	VkDescriptorBufferInfo points_info = {
		.buffer = r->storage_buffer,
		.offset = r->glyph_points_offset,
		.range = r->glyph_points_size,
	};

	VkWriteDescriptorSet writes[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = r->descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &glyph_info,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = r->descriptor_set,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &cells_info,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = r->descriptor_set,
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &points_info,
		},
	};

	vkUpdateDescriptorSets(r->device, 3, writes, 0, nullptr);
}

static void create_pipeline(Render *r)
{
	VkShaderModule vs_font = load_shader_module(r->device, "Resources/Shaders/Font.vert.spv");
	VkShaderModule fs_font = load_shader_module(r->device, "Resources/Shaders/Font.frag.spv");

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vs_font,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fs_font,
			.pName = "main",
		}
	};

	VkVertexInputBindingDescription vertex_input_binding = {
		0, sizeof(GlyphInstance), VK_VERTEX_INPUT_RATE_INSTANCE,
	};

	VkVertexInputAttributeDescription vertex_input_attributes[] = {
		{ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
		{ 1, 0, VK_FORMAT_R32_UINT, 16 },
		{ 2, 0, VK_FORMAT_R32_SFLOAT, 20 },
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.vertexBindingDescriptionCount = 1;
	vertex_input_state.pVertexBindingDescriptions = &vertex_input_binding;
	vertex_input_state.vertexAttributeDescriptionCount = 3;
	vertex_input_state.pVertexAttributeDescriptions = vertex_input_attributes;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)r->swapchain_extent.width;
	viewport.height = (float)r->swapchain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = r->swapchain_extent;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer_state = {};
	rasterizer_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_state.depthClampEnable = VK_FALSE;
	rasterizer_state.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_state.cullMode = VK_CULL_MODE_NONE;
	rasterizer_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer_state.depthBiasEnable = VK_FALSE;
	rasterizer_state.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blend_attachment_state = {};
	blend_attachment_state.blendEnable = VK_TRUE;
	blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
	blend_attachment_state.alphaBlendOp = VK_BLEND_OP_MAX;
	blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blend_sate = {};
	blend_sate.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_sate.logicOpEnable = VK_FALSE;
	blend_sate.logicOp = VK_LOGIC_OP_COPY;
	blend_sate.attachmentCount = 1;
	blend_sate.pAttachments = &blend_attachment_state;

	VkGraphicsPipelineCreateInfo pipeline_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = shader_stages,
			.pVertexInputState = &vertex_input_state,
			.pInputAssemblyState = &input_assembly_state,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterizer_state,
			.pMultisampleState = &multisampling,
			.pColorBlendState = &blend_sate,
			.layout = r->pipeline_layout,
			.renderPass = r->render_pass,
			.subpass = 0,
		},
	};

	CheckVk(vkCreateGraphicsPipelines(
		r->device,
		VK_NULL_HANDLE,
		1,
		pipeline_infos,
		nullptr,
		&r->pipeline));

	vkDestroyShaderModule(r->device, vs_font, nullptr);
	vkDestroyShaderModule(r->device, fs_font, nullptr);
}

static void create_swap_chain_objects(Render *r)
{
	create_image_views(r);
	create_render_pass(r);
	create_pipeline(r);
	create_framebuffers(r);

	create_command_buffers(r);
	create_command_buffer_fences(r);
}

static void create_vulkan_objects(Render *r)
{
	create_instance(r);
	create_surface(r);
	pick_physical_device(r);
	find_queue_families(r);
	create_device(r);
	create_command_pool(r);
	create_layout(r);
	create_storage_buffer(r);
	create_instance_buffer(r);
	create_descriptor_pool(r);
	create_descriptor_set(r);

	create_swap_chain(r);
	create_swap_chain_objects(r);
	create_semaphores(r);
}

static void destroy_swap_chain_objects(Render *r)
{
	vkDestroyPipeline(r->device, r->pipeline, nullptr);

	if (r->command_buffer_fences)
	{
		for (uint32_t i = 0; i < r->ring_buffer_count; i++)
		{
			vkDestroyFence(r->device, r->command_buffer_fences[i], nullptr);
		}
	}

	if (r->command_buffers)
	{
		vkFreeCommandBuffers(r->device, r->command_pool, r->ring_buffer_count, r->command_buffers.get());
	}

	if (r->framebuffers)
	{
		for (uint32_t i = 0; i < r->image_count; i++)
		{
			vkDestroyFramebuffer(r->device, r->framebuffers[i], nullptr);
		}
	}

	vkDestroyRenderPass(r->device, r->render_pass, nullptr);

	if (r->image_views)
	{
		for (uint32_t i = 0; i < r->image_count; i++)
		{
			vkDestroyImageView(r->device, r->image_views[i], nullptr);
		}
	}
}

static void destroy_vulkan_objects(Render *r)
{
//	vkDeviceWaitIdle(r->device);

	destroy_swap_chain_objects(r);

	vkDestroyBuffer(r->device, r->instance_staging_buffer, nullptr);
	vkFreeMemory(r->device, r->instance_staging_buffer_memory, nullptr);
	vkDestroyBuffer(r->device, r->instance_buffer, nullptr);
	vkFreeMemory(r->device, r->instance_buffer_memory, nullptr);
	vkDestroyBuffer(r->device, r->storage_buffer, nullptr);
	vkFreeMemory(r->device, r->storage_buffer_memory, nullptr);
	vkDestroyDescriptorPool(r->device, r->descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(r->device, r->set_layout, nullptr);
	vkDestroyPipelineLayout(r->device, r->pipeline_layout, nullptr);
	vkDestroySwapchainKHR(r->device, r->swapchain, nullptr);
	vkDestroySemaphore(r->device, r->image_available_semaphore, nullptr);
	vkDestroySemaphore(r->device, r->render_finished_semaphore, nullptr);
	vkDestroyCommandPool(r->device, r->command_pool, nullptr);

	vkDestroyDevice(r->device, nullptr);
	r->device = VK_NULL_HANDLE;

	vkDestroySurfaceKHR(r->instance, r->surface, nullptr);
	r->surface = VK_NULL_HANDLE;

	vkDestroyInstance(r->instance, nullptr);
	r->instance = VK_NULL_HANDLE;
}

static void recreate_swap_chain(Render *r)
{
	vkDeviceWaitIdle(r->device);

	destroy_swap_chain_objects(r);
	create_swap_chain(r);
	create_swap_chain_objects(r);
}

static void update(Render *r)
{
	r->frame++;
	r->fps_temp++;
	r->ring_buffer_index = r->frame % r->ring_buffer_count;

	auto curr = static_cast<float>(glfwGetTime());

	if (floorf(curr) > floorf(r->time))
	{
		r->fps = r->fps_temp;
		r->fps_temp = 0;
	}

	r->delta_time = (float)(curr - r->time);
	r->time = curr;

	double xpos, ypos;
	glfwGetCursorPos(r->window, &xpos, &ypos);

	r->mouse_pos[0] = (float)xpos;
	r->mouse_pos[1] = (float)ypos;

	acid::Vector2 mouse_delta = r->old_mouse_pos - r->mouse_pos;

	if (glfwGetMouseButton(r->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		mouse_delta *= 1.0f / r->canvas_scale;
		r->canvas_offset += mouse_delta;
	}

	r->old_mouse_pos = r->mouse_pos;

	acid::Vector2 swapchainExtent = {
		(float)r->swapchain_extent.width,
		(float)r->swapchain_extent.height
	};

	acid::Vector2 old_size = swapchainExtent * (1.0f / r->canvas_scale);

	if (r->canvas_scale != r->target_canvas_scale)
	{
		r->canvas_scale = acid::lerpf(r->canvas_scale, r->target_canvas_scale, r->delta_time * 30.0f);

		acid::Vector2 new_size = swapchainExtent * (1.0f / r->canvas_scale);

		acid::Vector2 tmp = old_size - new_size;

		tmp[0] *= r->mouse_pos[0] / swapchainExtent[0];
		tmp[1] *= r->mouse_pos[1] / swapchainExtent[1];

		r->canvas_offset += tmp;
	}
}

static void render_frame(Render *r)
{
	VkResult res = vkAcquireNextImageKHR(r->device, r->swapchain, UINT32_MAX, r->image_available_semaphore, VK_NULL_HANDLE, &r->image_index);

	if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreate_swap_chain(r);
		return;
	}
	else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
	{
		return;
	}

	VkFence waitFence = r->command_buffer_fences[r->ring_buffer_index];
	vkWaitForFences(r->device, 1, &waitFence, VK_TRUE, UINT64_MAX);
	vkResetFences(r->device, 1, &waitFence);

	record_current_command_buffer(r);

	VkSemaphore waitSemaphores[] = { r->image_available_semaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { r->render_finished_semaphore };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &r->command_buffers[r->ring_buffer_index];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	res = vkQueueSubmit(r->graphics_queue, 1, &submitInfo, waitFence);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &r->swapchain;
	presentInfo.pImageIndices = &r->image_index;
	res = vkQueuePresentKHR(r->present_queue, &presentInfo);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		recreate_swap_chain(r);
	}
}


static void on_window_resized(GLFWwindow* window, int width, int height)
{
	if (width == 0 || height == 0) return;

	HEIGHT = height;
	WIDTH = width;
	recreate_swap_chain((Render *)(glfwGetWindowUserPointer(window)));
}

void window_refresh_callback(GLFWwindow* window)
{
	Render *r = (Render *)(glfwGetWindowUserPointer(window));

	update(r);
	render_frame(r);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	Render *r = (Render *)(glfwGetWindowUserPointer(window));
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
	}
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Render *r = (Render *)(glfwGetWindowUserPointer(window));
	if (key == GLFW_KEY_ENTER)
	{
		r->ring_buffer_count = 2;
		r->canvas_scale = 3.0f;
		r->target_canvas_scale = 3.0f;
		r->canvas_offset = acid::Vector2();
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Render *r = (Render *)(glfwGetWindowUserPointer(window));

	r->target_canvas_scale *= powf(1.3f, (float)yoffset);
}

int main(int argc, const char **args)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

	Render render = {};
	render.ring_buffer_count = 2;
	render.canvas_scale = 3.0f;
	render.target_canvas_scale = 3.0f;
	render.window = window;

	glfwSetWindowUserPointer(window, &render);
	glfwSetWindowSizeCallback(window, on_window_resized);
	glfwSetWindowRefreshCallback(window, window_refresh_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetKeyCallback(window, key_callback);
	glfwSetScrollCallback(window, scroll_callback);

	create_vulkan_objects(&render);

	while (!glfwWindowShouldClose(window))
	{
		update(&render);
		render_frame(&render);
		glfwPollEvents();
	}

	destroy_vulkan_objects(&render);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
