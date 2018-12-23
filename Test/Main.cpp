#include "Main.hpp"

const uint32_t Main::MAX_VISIBLE_GLYPHS = 4096;
const uint32_t Main::NUMBER_OF_GLYPHS = 256;

void CallbackWindowResized(GLFWwindow *window, int width, int height)
{
	if (width == 0 || height == 0) return;

	Main *main = (Main *)(glfwGetWindowUserPointer(window));
	main->m_height = height;
	main->m_width = width;
	main->recreate_swap_chain();
}

void CallbackWindowRefresh(GLFWwindow *window)
{
	Main *main = (Main *)(glfwGetWindowUserPointer(window));
	main->Update();
	main->RenderFrame();
}

void CallbackMouseButton(GLFWwindow *window, int button, int action, int mods)
{
	auto main = (Main *)(glfwGetWindowUserPointer(window));
	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
	}
}

void CallbackKey(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	auto main = (Main *)(glfwGetWindowUserPointer(window));
	if (key == GLFW_KEY_ENTER)
	{
		main->m_ring_buffer_count = 2;
		main->m_canvas_scale = 3.0f;
		main->m_target_canvas_scale = 3.0f;
		main->m_canvas_offset = acid::Vector2();
	}
}

void CallbackScroll(GLFWwindow *window, double xoffset, double yoffset)
{
	auto main = (Main *)(glfwGetWindowUserPointer(window));
	main->m_target_canvas_scale *= powf(1.3f, (float)yoffset);
}

Main::Main(const uint32_t &width, const uint32_t &height) :
	m_width(width),
	m_height(height),
	m_outlines(new acid::Outline[NUMBER_OF_GLYPHS]),
	m_glyph_infos(new HostGlyphInfo[NUMBER_OF_GLYPHS]),
	m_swapchain(VK_NULL_HANDLE)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(m_width, m_height, "TTF Fonts", nullptr, nullptr);

	m_ring_buffer_count = 2;
	m_canvas_scale = 3.0f;
	m_target_canvas_scale = 3.0f;
	m_window = window;

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, CallbackWindowResized);
	glfwSetWindowRefreshCallback(window, CallbackWindowRefresh);
	glfwSetMouseButtonCallback(window, CallbackMouseButton);
	glfwSetKeyCallback(window, CallbackKey);
	glfwSetScrollCallback(window, CallbackScroll);

	create_vulkan_objects();
}

Main::~Main()
{
	destroy_vulkan_objects();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void Main::Run()
{
	while (!glfwWindowShouldClose(m_window))
	{
		Update();
		RenderFrame();
		glfwPollEvents();
	}
}

void Main::CheckVk(const VkResult &result)
{
	if (result >= 0)
	{
		return;
	}

	assert(false && "Vulkan error!");
}

uint32_t Main::align_uint32(const uint32_t &value, const uint32_t &alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

void Main::Update()
{
	m_frame++;
	m_fps_temp++;
	m_ring_buffer_index = m_frame % m_ring_buffer_count;

	auto curr = static_cast<float>(glfwGetTime());

	if (floorf(curr) > floorf(m_time))
	{
		m_fps = m_fps_temp;
		m_fps_temp = 0;
	}

	m_delta_time = (float)(curr - m_time);
	m_time = curr;

	double xpos, ypos;
	glfwGetCursorPos(m_window, &xpos, &ypos);

	m_mouse_pos[0] = (float)xpos;
	m_mouse_pos[1] = (float)ypos;

	acid::Vector2 mouse_delta = m_old_mouse_pos - m_mouse_pos;

	if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		mouse_delta *= 1.0f / m_canvas_scale;
		m_canvas_offset += mouse_delta;
	}

	m_old_mouse_pos = m_mouse_pos;

	acid::Vector2 swapchainExtent = {
		(float)m_swapchain_extent.width,
		(float)m_swapchain_extent.height
	};

	acid::Vector2 old_size = swapchainExtent * (1.0f / m_canvas_scale);

	if (m_canvas_scale != m_target_canvas_scale)
	{
		m_canvas_scale = acid::lerpf(m_canvas_scale, m_target_canvas_scale, m_delta_time * 30.0f);

		acid::Vector2 new_size = swapchainExtent * (1.0f / m_canvas_scale);

		acid::Vector2 tmp = old_size - new_size;

		tmp[0] *= m_mouse_pos[0] / swapchainExtent[0];
		tmp[1] *= m_mouse_pos[1] / swapchainExtent[1];

		m_canvas_offset += tmp;
	}
}

void Main::RenderFrame()
{
	VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT32_MAX, m_image_available_semaphore, VK_NULL_HANDLE, &m_image_index);

	if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreate_swap_chain();
		return;
	}
	else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
	{
		return;
	}

	VkFence waitFence = m_command_buffer_fences[m_ring_buffer_index];
	vkWaitForFences(m_device, 1, &waitFence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_device, 1, &waitFence);

	record_current_command_buffer();

	VkSemaphore waitSemaphores[] = { m_image_available_semaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { m_render_finished_semaphore };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_command_buffers[m_ring_buffer_index];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	res = vkQueueSubmit(m_graphics_queue, 1, &submitInfo, waitFence);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &m_image_index;
	res = vkQueuePresentKHR(m_present_queue, &presentInfo);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
	{
		recreate_swap_chain();
	}
}

void Main::load_font()
{
	FT_Library library;
	FT_CHECK(FT_Init_FreeType(&library));

	FT_Face face;
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/Pacifico-Regular.ttf", 0, &face));
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/Lato-Medium.ttf", 0, &face));
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/Lobster-Regular.ttf", 0, &face));
	FT_CHECK(FT_New_Face(library, "Resources/Fonts/LobsterTwo-Italic.ttf", 0, &face));
//	FT_CHECK(FT_New_Face(library, "Resources/Fonts/OpenSans-Regular.ttf", 0, &face));

	FT_CHECK(FT_Set_Char_Size(face, 0, 1000 * 64, 96, 96));

	uint32_t total_points = 0;
	uint32_t total_cells = 0;

	/*FT_ULong charcode;
	FT_UInt gindex;
	charcode = FT_Get_First_Char(face, &gindex);

	while (gindex != 0)
	{
		acid::Outline &o = m_outlines[gindex];
		HostGlyphInfo &hgi = m_glyph_infos[gindex];

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

		acid::Outline *o = &m_outlines[i];
		HostGlyphInfo *hgi = &m_glyph_infos[i];

		FT_UInt glyph_index = FT_Get_Char_Index(face, c);
		FT_CHECK(FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_HINTING));

		o->Convert(&face->glyph->outline);

		hgi->bbox = o->m_bbox;
		hgi->advance = face->glyph->metrics.horiAdvance / 64.0f;

		total_points += o->m_pointCount;
		total_cells += o->m_cellCountX * o->m_cellCountY;
	}

	m_glyph_info_size = sizeof(DeviceGlyphInfo) * NUMBER_OF_GLYPHS;
	m_glyph_cells_size = sizeof(uint32_t) * total_cells;
	m_glyph_points_size = sizeof(acid::Vector2) * total_points;

	uint32_t alignment = m_device_properties.limits.minStorageBufferOffsetAlignment;
	m_glyph_info_offset = 0;
	m_glyph_cells_offset = align_uint32(m_glyph_info_size, alignment);
	m_glyph_points_offset = align_uint32(m_glyph_info_size + m_glyph_cells_size, alignment);
	m_glyph_data_size = m_glyph_points_offset + m_glyph_points_size;

	m_glyph_data = malloc(m_glyph_data_size);

	DeviceGlyphInfo *device_glyph_infos = (DeviceGlyphInfo*)((char*)m_glyph_data + m_glyph_info_offset);
	uint32_t *cells = (uint32_t*)((char*)m_glyph_data + m_glyph_cells_offset);
	acid::Vector2 *points = (acid::Vector2*)((char*)m_glyph_data + m_glyph_points_offset);

	uint32_t point_offset = 0;
	uint32_t cell_offset = 0;

	for (uint32_t i = 0; i < NUMBER_OF_GLYPHS; i++)
	{
		acid::Outline *o = &m_outlines[i];
		DeviceGlyphInfo *dgi = &device_glyph_infos[i];

		dgi->cell_info.cell_count_x = o->m_cellCountX;
		dgi->cell_info.cell_count_y = o->m_cellCountY;
		dgi->cell_info.point_offset = point_offset;
		dgi->cell_info.cell_offset = cell_offset;
		dgi->bbox = o->m_bbox;

		uint32_t cell_count = o->m_cellCountX * o->m_cellCountY;
		memcpy(cells + cell_offset, o->m_cells, sizeof(uint32_t) * cell_count);
		memcpy(points + point_offset, o->m_points, sizeof(acid::Vector2) * o->m_pointCount);

		point_offset += o->m_pointCount;
		cell_offset += cell_count;
	}

	assert(point_offset == total_points);
	assert(cell_offset == total_cells);

	for (uint32_t i = 0; i < NUMBER_OF_GLYPHS; i++)
	{
		m_outlines[i].Destroy();
	}

	FT_CHECK(FT_Done_Face(face));
	FT_CHECK(FT_Done_FreeType(library));
}

void Main::create_instance()
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

	CheckVk(vkCreateInstance(&instance_info, nullptr, &m_instance));
}

void Main::create_surface()
{
	CheckVk(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface));
}

void Main::pick_physical_device()
{
	uint32_t physical_device_count;
	CheckVk(vkEnumeratePhysicalDevices(m_instance, &physical_device_count, nullptr));
	assert(physical_device_count > 0);

	std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
	CheckVk(vkEnumeratePhysicalDevices(m_instance, &physical_device_count, physical_devices.data()));

	m_physical_device = physical_devices[0];

	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);
	vkGetPhysicalDeviceProperties(m_physical_device, &m_device_properties);
}

void Main::create_device()
{
	char const * extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = nullptr,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = extensions,
	};

	float queue_priority = 1.0f;

	if (m_graphics_queue_family == m_present_queue_family)
	{
		VkDeviceQueueCreateInfo queue_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.queueFamilyIndex = m_graphics_queue_family,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority,
		};

		device_info.pQueueCreateInfos = &queue_info;
		device_info.queueCreateInfoCount = 1;

		CheckVk(vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device));
	}
	else
	{
		VkDeviceQueueCreateInfo queue_infos[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = nullptr,
				.queueFamilyIndex = m_graphics_queue_family,
				.queueCount = 1,
				.pQueuePriorities = &queue_priority,
			},
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = nullptr,
				.queueFamilyIndex = m_present_queue_family,
				.queueCount = 1,
				.pQueuePriorities = &queue_priority,
			},
		};

		device_info.pQueueCreateInfos = queue_infos;
		device_info.queueCreateInfoCount = 2;

		CheckVk(vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device));
	}

	vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
	vkGetDeviceQueue(m_device, m_present_queue_family, 0, &m_present_queue);
}

void Main::find_queue_families()
{
	uint32_t family_count;

	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &family_count, nullptr);

	std::vector<VkQueueFamilyProperties> properties(family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &family_count, properties.data());

	m_graphics_queue_family = UINT32_MAX;
	m_present_queue_family = UINT32_MAX;

	for (uint32_t i = 0; i < family_count; i++)
	{
		if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			VkBool32 present;
			CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface, &present));

			if (present)
			{
				m_graphics_queue_family = i;
				m_present_queue_family = i;
				break;
			}
			else
			{
				if (m_graphics_queue_family == UINT32_MAX)
					m_graphics_queue_family = i;
			}
		}
	}

	if (m_present_queue_family == UINT32_MAX)
	{
		for (uint32_t i = 0; i < family_count; i++)
		{
			VkBool32 present;
			CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface, &present));

			if (present)
			{
				m_present_queue_family = i;
				break;
			}
		}
	}
}

void Main::choose_surface_format()
{
	uint32_t format_count;
	CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(
		m_physical_device, m_surface, &format_count, nullptr));

	assert(format_count > 0);

	std::vector<VkSurfaceFormatKHR> formats(format_count);
	CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(
		m_physical_device, m_surface, &format_count, formats.data()));
	/*
	if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		m_format = VK_FORMAT_B8G8R8A8_UNORM;
	else m_format = formats[0].format;
	*/
	m_format = VK_FORMAT_B8G8R8A8_SRGB;
}

void Main::choose_present_mode()
{
	uint32_t present_mode_count;
	CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(
		m_physical_device, m_surface, &present_mode_count, nullptr));

	assert(present_mode_count > 0);

	std::vector<VkPresentModeKHR>	present_modes(present_mode_count);
	CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(
		m_physical_device, m_surface, &present_mode_count, present_modes.data()));


	for (uint32_t i = 0; i < present_mode_count; i++)
	{
		if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			return;
		}
	}

	//m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
	m_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
}

void Main::create_swap_chain()
{
	VkSurfaceCapabilitiesKHR capabilities;
	CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &capabilities));

	uint32_t min_image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount && min_image_count > capabilities.maxImageCount)
		min_image_count = capabilities.maxImageCount;

	choose_surface_format();
	choose_present_mode();

	VkSwapchainKHR old_swapchain = m_swapchain;
	m_swapchain = nullptr;

	VkSwapchainCreateInfoKHR swapchain_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = m_surface,
		.minImageCount = min_image_count,
		.imageFormat = m_format,
		.imageColorSpace = m_color_space,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = m_present_mode,
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
		glfwGetWindowSize(m_window, &window_w, &window_h);

		uint32_t w, h;
		w = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, (uint32_t)window_w));
		h = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, (uint32_t)window_h));

		swapchain_info.imageExtent.width = w;
		swapchain_info.imageExtent.height = h;
	}

	m_swapchain_extent = swapchain_info.imageExtent;

	if (m_graphics_queue_family != m_present_queue_family)
	{
		uint32_t families[] = { m_graphics_queue_family, m_present_queue_family };
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

	CheckVk(vkCreateSwapchainKHR(m_device, &swapchain_info, nullptr, &m_swapchain));

	if (old_swapchain)
		vkDestroySwapchainKHR(m_device, old_swapchain, nullptr);

	CheckVk(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_image_count, nullptr));

	m_images = std::make_unique<VkImage[]>(m_image_count);
	CheckVk(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_image_count, m_images.get()));
}

void Main::create_image_views()
{
	m_image_views = std::make_unique<VkImageView[]>(m_image_count);
	memset(m_image_views.get(), 0, sizeof(VkImageView) * m_image_count);

	for (uint32_t i = 0; i < m_image_count; i++)
	{
		VkImageViewCreateInfo ci = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_format,
			.components = { },
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		CheckVk(vkCreateImageView(m_device, &ci, nullptr, &m_image_views[i]));
	}
}

void Main::create_render_pass()
{
	VkAttachmentDescription attachments[] = {
		{
			.format = m_format,
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

	CheckVk(vkCreateRenderPass(m_device, &ci, nullptr, &m_render_pass));
}

void Main::create_framebuffers()
{
	m_framebuffers = std::make_unique<VkFramebuffer[]>(m_image_count);
	memset(m_framebuffers.get(), 0, sizeof(VkFramebuffer) * m_image_count);

	for (uint32_t i = 0; i < m_image_count; i++)
	{
		VkImageView attachments[] = {
			m_image_views[i],
		};

		VkFramebufferCreateInfo ci = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = m_render_pass,
			.attachmentCount = 1,
			.pAttachments = attachments,
			.width = m_swapchain_extent.width,
			.height = m_swapchain_extent.height,
			.layers = 1,
		};

		CheckVk(vkCreateFramebuffer(m_device, &ci, nullptr, &m_framebuffers[i]));
	}
}

void Main::create_command_pool()
{
	VkCommandPoolCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_graphics_queue_family,
	};

	CheckVk(vkCreateCommandPool(m_device, &ci, nullptr, &m_command_pool));
}

void Main::begin_text()
{
	m_glyph_instance_count = 0;

	uint32_t size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance);
	uint32_t offset = size * m_ring_buffer_index;

	CheckVk(vkMapMemory(m_device, m_instance_staging_buffer_memory, offset, size, 0, (void **)&m_glyph_instances));
}

void Main::end_text()
{
	VkCommandBuffer cmd_buf = m_command_buffers[m_ring_buffer_index];
	uint32_t size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance);
	uint32_t offset = size * m_ring_buffer_index;

	vkUnmapMemory(m_device, m_instance_staging_buffer_memory);

	VkBufferMemoryBarrier barrier = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.buffer = m_instance_buffer,
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

	vkCmdCopyBuffer(cmd_buf, m_instance_staging_buffer, m_instance_buffer, 1, &copy);

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

void Main::append_text(float x, float y, float scale, const std::string &text)
{
	for (char c : text)
	{
		if (m_glyph_instance_count >= MAX_VISIBLE_GLYPHS)
			break;

		uint32_t glyph_index = static_cast<uint32_t>(c) - 32;

		HostGlyphInfo *gi = &m_glyph_infos[glyph_index];
		GlyphInstance *inst = &m_glyph_instances[m_glyph_instance_count];

		inst->rect.minX = (x + gi->bbox.minX * scale) / (m_swapchain_extent.width / 2.0f) - 1.0f;
		inst->rect.minY = (y - gi->bbox.minY * scale) / (m_swapchain_extent.height / 2.0f) - 1.0f;
		inst->rect.maxX = (x + gi->bbox.maxX * scale) / (m_swapchain_extent.width / 2.0f) - 1.0f;
		inst->rect.maxY = (y - gi->bbox.maxY * scale) / (m_swapchain_extent.height / 2.0f) - 1.0f;

		if (inst->rect.minX <= 1 && inst->rect.maxX >= -1 &&
		    inst->rect.maxY <= 1 && inst->rect.minY >= -1)
		{
			inst->glyph_index = glyph_index;
			inst->sharpness = scale;

			m_glyph_instance_count++;
		}

		x += gi->advance * scale;
	}
}

void Main::record_current_command_buffer()
{
	//printf("%d\n", m_ring_buffer_index);
	VkCommandBuffer cmd_buf = m_command_buffers[m_ring_buffer_index];

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
	};

	CheckVk(vkBeginCommandBuffer(cmd_buf, &begin_info));

	begin_text();

	append_text(5.0f, m_height - 10.0f, 0.02f, "Frame time: " + std::to_string((int32_t)(1000.0f / m_fps)) + " ms");
	append_text(5.0f, m_height - 40.0f, 0.02f, "FPS: " + std::to_string(m_fps));

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
		append_text(m_canvas_scale * (10.0f - m_canvas_offset[0]),
		            m_canvas_scale * (30.0f - m_canvas_offset[1] + i * 30.0f),
		            0.02f * m_canvas_scale,
		            lines[i]);
	}

	end_text();

	VkClearValue clear_value = { 1.0f, 1.0f, 1.0f, 1.0f }; // 0.1f, 0.2f, 0.3f
	VkRenderPassBeginInfo render_pass_bi = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_render_pass,
		.framebuffer = m_framebuffers[m_image_index],
		.renderArea = {
			.offset = { 0, 0 },
			.extent = m_swapchain_extent
		},
		.clearValueCount = 1,
		.pClearValues = &clear_value
	};

	vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd_buf, 0, 1, &m_instance_buffer, offsets);

	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
	                        m_pipeline_layout, 0, 1, &m_descriptor_set, 0, nullptr);

	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdDraw(cmd_buf, 4, m_glyph_instance_count, 0, 0);

	vkCmdEndRenderPass(cmd_buf);
	CheckVk(vkEndCommandBuffer(cmd_buf));
}

void Main::record_command_buffers()
{
	for (uint32_t i = 0; i < m_ring_buffer_count; i++)
	{
		m_ring_buffer_index = i;
		record_current_command_buffer();
	}
}

void Main::create_command_buffers()
{
	m_command_buffers = std::make_unique<VkCommandBuffer[]>(m_ring_buffer_count);
	memset(m_command_buffers.get(), 0, sizeof(VkCommandBuffer) * m_ring_buffer_count);

	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = m_ring_buffer_count,
	};

	CheckVk(vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.get()));
	record_command_buffers();
}

void Main::create_command_buffer_fences()
{
	m_command_buffer_fences = std::make_unique<VkFence[]>(m_ring_buffer_count);

	VkFenceCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (uint32_t i = 0; i < m_ring_buffer_count; i++)
		CheckVk(vkCreateFence(m_device, &ci, nullptr, &m_command_buffer_fences[i]));
}

void Main::create_semaphores()
{
	VkSemaphoreCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	CheckVk(vkCreateSemaphore(m_device, &ci, nullptr, &m_image_available_semaphore));
	CheckVk(vkCreateSemaphore(m_device, &ci, nullptr, &m_render_finished_semaphore));
}

VkShaderModule Main::load_shader_module(VkDevice device, const char *path)
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

void Main::create_layout()
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

	CheckVk(vkCreateDescriptorSetLayout(m_device, &layout_ci, nullptr, &m_set_layout));

	VkPipelineLayoutCreateInfo pipeline_ci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &m_set_layout,
	};

	CheckVk(vkCreatePipelineLayout(m_device, &pipeline_ci, nullptr, &m_pipeline_layout));
}

uint32_t Main::find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags flags)
{
	for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; i++)
	{
		if (type_bits & 1 << i)
		{
			VkMemoryPropertyFlags f = m_memory_properties.memoryTypes[i].propertyFlags;

			if ((f & flags) == flags)
				return i;
		}
	}

	return UINT32_MAX;
}

VkDeviceMemory Main::alloc_required_memory(VkMemoryRequirements *req, VkMemoryPropertyFlags flags)
{
	VkMemoryAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = req->size,
		.memoryTypeIndex = find_memory_type(req->memoryTypeBits, flags),
	};

	VkDeviceMemory mem;
	CheckVk(vkAllocateMemory(m_device, &alloc_info, nullptr, &mem));
	return mem;
}

void Main::create_buffer_with_memory(VkBufferCreateInfo *ci, VkMemoryPropertyFlags flags, VkDeviceMemory *memory,
                                         VkBuffer *buffer)
{
	CheckVk(vkCreateBuffer(m_device, ci, nullptr, buffer));

	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(m_device, *buffer, &req);

	*memory = alloc_required_memory(&req, flags);
	CheckVk(vkBindBufferMemory(m_device, *buffer, *memory, 0));
}

VkCommandBuffer Main::begin_one_time_cmdbuf()
{
	VkCommandBufferAllocateInfo cmd_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VkCommandBuffer cmd_buffer;
	CheckVk(vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &cmd_buffer));

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	CheckVk(vkBeginCommandBuffer(cmd_buffer, &begin_info));
	return cmd_buffer;
}

void Main::end_one_time_cmdbuf(VkCommandBuffer cmd_buffer)
{
	CheckVk(vkEndCommandBuffer(cmd_buffer));

	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd_buffer,
	};

	CheckVk(vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
	CheckVk(vkQueueWaitIdle(m_graphics_queue));
	vkFreeCommandBuffers(m_device, m_command_pool, 1, &cmd_buffer);
}

void Main::copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
	VkCommandBuffer cmd_buf = begin_one_time_cmdbuf();
	VkBufferCopy copy = { 0, 0, size };

	vkCmdCopyBuffer(cmd_buf, src_buffer, dst_buffer, 1, &copy);
	end_one_time_cmdbuf(cmd_buf);
}

void Main::stage_buffer(VkBuffer buffer, void *data, size_t size)
{
	VkBufferCreateInfo staging_ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	create_buffer_with_memory(&staging_ci,
	                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
	                          &staging_buffer_memory, &staging_buffer);

	void *staging_buffer_ptr;
	CheckVk(vkMapMemory(m_device, staging_buffer_memory, 0, staging_ci.size, 0, &staging_buffer_ptr));

	memcpy(staging_buffer_ptr, data, size);

	vkUnmapMemory(m_device, staging_buffer_memory);

	copy_buffer(staging_buffer, buffer, size);

	vkDestroyBuffer(m_device, staging_buffer, nullptr);
	vkFreeMemory(m_device, staging_buffer_memory, nullptr);
}

void Main::create_storage_buffer()
{
	load_font();

	VkBufferCreateInfo storage_ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = m_glyph_data_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	create_buffer_with_memory(&storage_ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	                          &m_storage_buffer_memory, &m_storage_buffer);

	stage_buffer(m_storage_buffer, m_glyph_data, storage_ci.size);

	free(m_glyph_data);
	m_glyph_data = nullptr;
}

void Main::create_instance_buffer()
{
	VkBufferCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance),
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	create_buffer_with_memory(&ci, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	                          &m_instance_buffer_memory, &m_instance_buffer);

	VkBufferCreateInfo staging_ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = ci.size * m_ring_buffer_count,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	create_buffer_with_memory(&staging_ci,
	                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
	                          &m_instance_staging_buffer_memory, &m_instance_staging_buffer);
}

void Main::create_descriptor_pool()
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

	CheckVk(vkCreateDescriptorPool(m_device, &ci, nullptr, &m_descriptor_pool));
}

void Main::create_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = m_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_set_layout,
	};

	CheckVk(vkAllocateDescriptorSets(m_device, &alloc_info, &m_descriptor_set));

	VkDescriptorBufferInfo glyph_info = {
		.buffer = m_storage_buffer,
		.offset = m_glyph_info_offset,
		.range = m_glyph_info_size,
	};

	VkDescriptorBufferInfo cells_info = {
		.buffer = m_storage_buffer,
		.offset = m_glyph_cells_offset,
		.range = m_glyph_cells_size,
	};

	VkDescriptorBufferInfo points_info = {
		.buffer = m_storage_buffer,
		.offset = m_glyph_points_offset,
		.range = m_glyph_points_size,
	};

	VkWriteDescriptorSet writes[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &glyph_info,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_descriptor_set,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &cells_info,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_descriptor_set,
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &points_info,
		},
	};

	vkUpdateDescriptorSets(m_device, 3, writes, 0, nullptr);
}

void Main::create_pipeline()
{
	VkShaderModule vs_font = load_shader_module(m_device, "Resources/Shaders/Font.vert.spv");
	VkShaderModule fs_font = load_shader_module(m_device, "Resources/Shaders/Font.frag.spv");

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
	viewport.width = (float)m_swapchain_extent.width;
	viewport.height = (float)m_swapchain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = m_swapchain_extent;

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
			.layout = m_pipeline_layout,
			.renderPass = m_render_pass,
			.subpass = 0,
		},
	};

	CheckVk(vkCreateGraphicsPipelines(
		m_device,
		VK_NULL_HANDLE,
		1,
		pipeline_infos,
		nullptr,
		&m_pipeline));

	vkDestroyShaderModule(m_device, vs_font, nullptr);
	vkDestroyShaderModule(m_device, fs_font, nullptr);
}

void Main::create_swap_chain_objects()
{
	create_image_views();
	create_render_pass();
	create_pipeline();
	create_framebuffers();

	create_command_buffers();
	create_command_buffer_fences();
}

void Main::create_vulkan_objects()
{
	create_instance();
	create_surface();
	pick_physical_device();
	find_queue_families();
	create_device();
	create_command_pool();
	create_layout();
	create_storage_buffer();
	create_instance_buffer();
	create_descriptor_pool();
	create_descriptor_set();

	create_swap_chain();
	create_swap_chain_objects();
	create_semaphores();
}

void Main::destroy_swap_chain_objects()
{
	vkDestroyPipeline(m_device, m_pipeline, nullptr);

	if (m_command_buffer_fences)
	{
		for (uint32_t i = 0; i < m_ring_buffer_count; i++)
		{
			vkDestroyFence(m_device, m_command_buffer_fences[i], nullptr);
		}
	}

	if (m_command_buffers)
	{
		vkFreeCommandBuffers(m_device, m_command_pool, m_ring_buffer_count, m_command_buffers.get());
	}

	if (m_framebuffers)
	{
		for (uint32_t i = 0; i < m_image_count; i++)
		{
			vkDestroyFramebuffer(m_device, m_framebuffers[i], nullptr);
		}
	}

	vkDestroyRenderPass(m_device, m_render_pass, nullptr);

	if (m_image_views)
	{
		for (uint32_t i = 0; i < m_image_count; i++)
		{
			vkDestroyImageView(m_device, m_image_views[i], nullptr);
		}
	}
}

void Main::destroy_vulkan_objects()
{
//	vkDeviceWaitIdle(m_device);

	destroy_swap_chain_objects();

	vkDestroyBuffer(m_device, m_instance_staging_buffer, nullptr);
	vkFreeMemory(m_device, m_instance_staging_buffer_memory, nullptr);
	vkDestroyBuffer(m_device, m_instance_buffer, nullptr);
	vkFreeMemory(m_device, m_instance_buffer_memory, nullptr);
	vkDestroyBuffer(m_device, m_storage_buffer, nullptr);
	vkFreeMemory(m_device, m_storage_buffer_memory, nullptr);
	vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_set_layout, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	vkDestroySemaphore(m_device, m_image_available_semaphore, nullptr);
	vkDestroySemaphore(m_device, m_render_finished_semaphore, nullptr);
	vkDestroyCommandPool(m_device, m_command_pool, nullptr);

	vkDestroyDevice(m_device, nullptr);
	m_device = VK_NULL_HANDLE;

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	m_surface = VK_NULL_HANDLE;

	vkDestroyInstance(m_instance, nullptr);
	m_instance = VK_NULL_HANDLE;
}

void Main::recreate_swap_chain()
{

	vkDeviceWaitIdle(m_device);

	destroy_swap_chain_objects();
	create_swap_chain();
	create_swap_chain_objects();
}


int main(int argc, const char **args)
{
	Main main = Main(1080, 720);
	main.Run();
	return 0;
}
