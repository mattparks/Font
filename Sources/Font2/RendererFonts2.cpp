#include "RendererFonts2.hpp"

#include <Models/VertexModel.hpp>
#include <Models/Shapes/ModelRectangle.hpp>
#include <Scenes/Scenes.hpp>
#include <Uis/Uis.hpp>
#include <Renderer/Renderer.hpp>

namespace acid
{
	static const uint32_t MAX_VISIBLE_GLYPHS = 4096;

	RendererFonts2::RendererFonts2(const Pipeline::Stage &pipelineStage) :
		RenderPipeline(pipelineStage),
		m_pipeline(PipelineGraphics(pipelineStage, {"Shaders/Fonts2/Font.vert", "Shaders/Fonts2/Font.frag"}, {GetVertexInput()},
			PipelineGraphics::Mode::Polygon, PipelineGraphics::Depth::None, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false, {}))
	//	m_descriptorSet(DescriptorsHandler()),
	//	m_storageGlyphs(nullptr),
	//	m_instanceBuffer(InstanceBuffer(MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance)))
	{
		Log::Out("%s\n", m_pipeline.GetShaderProgram()->ToString().c_str());

	//	std::string filename = "Alice-Regular.ttf";
	//	std::string filename = "marediv.ttf";
	//	std::string filename = "Lobster-Regular.ttf";
	//	std::string filename = "LobsterTwo-Bold.ttf";
	//	std::string filename = "LobsterTwo-BoldItalic.ttf";
	//	std::string filename = "LobsterTwo-Italic.ttf";
	//	std::string filename = "LobsterTwo-Regular.ttf";
	//	std::string filename = "OpenSans-Bold.ttf";
	//	std::string filename = "OpenSans-BoldItalic.ttf";
	//	std::string filename = "OpenSans-ExtraBold.ttf";
	//	std::string filename = "OpenSans-ExtraBoldItalic.ttf";
	//	std::string filename = "OpenSans-Italic.ttf";
	//	std::string filename = "OpenSans-Light.ttf";
	//	std::string filename = "OpenSans-LightItalic.ttf";
	//	std::string filename = "OpenSans-Regular.ttf";
	//	std::string filename = "OpenSans-SemiBold.ttf";
	//	std::string filename = "OpenSans-SemiBoldItalic.ttf";
	//	std::string filename = "Roboto-Black.ttf";
	//	std::string filename = "Roboto-BlackItalic.ttf";
	//	std::string filename = "Roboto-Bold.ttf";
	//	std::string filename = "Roboto-BoldItalic.ttf";
	//	std::string filename = "Roboto-Italic.ttf";
	//	std::string filename = "Roboto-Light.ttf";
	//	std::string filename = "Roboto-LightItalic.ttf";
		std::string filename = "Roboto-Medium.ttf";
	//	std::string filename = "Roboto-MediumItalic.ttf";
	//	std::string filename = "Roboto-Regular.ttf";
	//	std::string filename = "Roboto-Thin.ttf";
	//	std::string filename = "Roboto-ThinItalic.ttf";
		LoadFont("Resources/Game/Fonts/" + filename);
		CreateStorageBuffer();
		CreateInstanceBuffer();
		CreateDescriptorSet();
	}

	void RendererFonts2::Render(const CommandBuffer &commandBuffer)
	{
		auto logicalDevice = Renderer::Get()->GetLogicalDevice();

		glyphInstanceCount = 0;
	//	m_instanceBuffer.Map(reinterpret_cast<void **>(&glyphInstances));
		uint32_t size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance);
		Renderer::CheckVk(vkMapMemory(logicalDevice->GetLogicalDevice(), instanceStagingBufferMemory, 0, size, 0, reinterpret_cast<void **>(&glyphInstances)));

		static std::vector<std::wstring> lines = {
			L"Hello world, Привет мир, schön! 0123456789 #$%^*@&( []{} «»½¼±¶§",
			L"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus sit amet scelerisque augue, sit amet commodo neque. Vestibulum",
			L"eu eros a justo molestie bibendum quis in urna. Integer quis tristique magna. Morbi in ultricies lorem. Donec lacinia nisi et",
			L"arcu scelerisque, eget viverra ante dapibus. Proin enim neque, vehicula id congue quis, consequat sit amet tortor.Aenean ac",
			L"lorem sit amet magna rhoncus rhoncus ac ac neque. Cras sed rutrum sem. Donec placerat ultricies ex, a gravida lorem commodo ut.",
			L"Mauris faucibus aliquet ligula, vitae condimentum dui semper et. Aenean pellentesque ac ligula a varius. Suspendisse congue",
			L"lorem lorem, ac consectetur ipsum condimentum id.",
			L"",
			L"Vestibulum quis erat sem. Fusce efficitur libero et leo sagittis, ac volutpat felis ullamcorper. Curabitur fringilla eros eget ex",
			L"lobortis, at posuere sem consectetur. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis",
			L"egestas. Vivamus eu enim leo. Morbi ultricies lorem et pellentesque vestibulum. Proin eu ultricies sem. Quisque laoreet, ligula",
			L"non molestie congue, odio nunc tempus arcu, vel aliquet leo turpis non enim. Sed risus dui, condimentum et eros a, molestie",
			L"imperdiet nisl. Vivamus quis ante venenatis, cursus magna ut, tincidunt elit. Aenean nisl risus, porttitor et viverra quis,",
			L"tempus vitae nisl.",
			L"",
			L"Suspendisse ut scelerisque tellus. In ac quam sem.Curabitur suscipit massa nisl. Ut et metus sed lacus dapibus molestie. Nullam",
			L"porttitor sit amet magna quis dapibus. Nulla tincidunt, arcu sit amet hendrerit consequat, felis leo blandit libero, eu posuere",
			L"nisl quam interdum nulla. Quisque nec efficitur libero. Quisque quis orci vitae metus feugiat aliquam eu et nulla. Etiam aliquet",
			L"ante vitae lacus aliquam, et gravida elit mollis. Proin molestie, justo tempus rhoncus aliquam, tellus erat venenatis erat,",
			L"porttitor dapibus nunc purus id enim. Integer a nunc ut velit porta maximus. Nullam rutrum nisi in sagittis pharetra. Proin id",
			L"pharetra augue, sed vulputate lorem. Aenean dapibus, turpis nec ullamcorper pharetra, ex augue congue nibh, condimentum",
			L"vestibulum arcu urna quis ex.",
			L"",
			L"Vestibulum non dignissim nibh, quis vestibulum odio. Ut sed viverra ante, fringilla convallis tellus. Donec in est rutrum,",
			L"imperdiet dolor a, vestibulum magna. In nec justo tellus. Ut non erat eu leo ornare imperdiet in sit amet lorem. Nullam quis",
			L"nisl diam. Aliquam laoreet dui et ligula posuere cursus.",
			L"",
			L"Donec vestibulum ante eget arcu dapibus lobortis.Curabitur condimentum tellus felis, id luctus mi ultrices quis. Aenean nulla",
			L"justo, venenatis vel risus et, suscipit faucibus nulla. Pellentesque habitant morbi tristique senectus et netus et malesuada",
			L"fames ac turpis egestas. Sed lacinia metus eleifend lacinia blandit.Morbi est nibh, dapibus nec arcu quis, volutpat lacinia",
			L"dolor. Vestibulum quis viverra erat.Maecenas ultricies odio neque, et eleifend arcu auctor in. Suspendisse placerat massa nisl,",
			L"non condimentum ligula sodales at.Phasellus eros urna, elementum in ultricies quis, vulputate id magna. Donec efficitur rutrum",
			L"urna sed tempus. Vestibulum eu augue dolor. Vestibulum vehicula suscipit purus, sit amet ultricies ligula malesuada sit amet.",
			L"Duis consectetur elit euismod arcu aliquet vehicula. Pellentesque lobortis dui et nisl vehicula, in placerat quam dapibus.Fusce",
			L"auctor arcu a purus bibendum, eget blandit nisi lobortis."
		};

		for (uint32_t i = 0; i < lines.size(); i++)
		{
			AppendText(3.0f * (10.0f - 0.0f),
			           3.0f * (30.0f - 0.0f + i * 30.0f),
			           0.02f * 3.0f, lines[i], Colour::Black);
		}

		AppendText(5.0f, 25.0f, 0.02f, "Frame Time: " + String::To(Engine::Get()->GetDelta().AsMilliseconds()) + "ms", Colour::Red);
		AppendText(5.0f, 55.0f, 0.02f, "Fps: " + String::To(1.0f / Engine::Get()->GetDelta().AsSeconds()), Colour::Green);

	//	m_instanceBuffer.Unmap(commandBuffer);
		{
			vkUnmapMemory(logicalDevice->GetLogicalDevice(), instanceStagingBufferMemory);

			VkBufferMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.buffer = instanceBuffer;
			barrier.offset = 0;
			barrier.size = size;

			vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(), VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                     0, 0, nullptr, 1, &barrier, 0, nullptr);

			VkBufferCopy copy = {};
			copy.srcOffset = 0;
			copy.dstOffset = 0;
			copy.size = size;

			vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), instanceStagingBuffer, instanceBuffer, 1, &copy);

			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer.GetCommandBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			                     0, 0, nullptr, 1, &barrier, 0, nullptr);
		}

		m_pipeline.BindPipeline(commandBuffer);

		// Updates descriptors.
		/*m_descriptorSet.Push("GlyphBuffer", *m_storageGlyphs, OffsetSize(glyphInfoOffset, glyphInfoSize));
		m_descriptorSet.Push("CellBuffer", *m_storageGlyphs, OffsetSize(glyphCellsOffset, glyphCellsSize));
		m_descriptorSet.Push("PointBuffer", *m_storageGlyphs, OffsetSize(glyphPointsOffset, glyphPointsSize));
		bool updateSuccess = m_descriptorSet.Update(m_pipeline);

		if (!updateSuccess)
		{
			return;
		}

		// Draws the object.
		m_descriptorSet.BindDescriptor(commandBuffer, m_pipeline);*/

		vkCmdBindDescriptorSets(commandBuffer.GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.GetPipelineLayout(), 0, 1,
			&descriptorSet, 0, nullptr);
		
	//	VkDeviceSize offsets[] = {0};
	//	vkCmdBindVertexBuffers(commandBuffer.GetCommandBuffer(), 0, 1, &m_instanceBuffer.GetBuffer(), offsets);
	//	vkCmdDraw(commandBuffer.GetCommandBuffer(), 4, glyphInstanceCount, 0, 0);

		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer.GetCommandBuffer(), 0, 1, &instanceBuffer, offsets);
		vkCmdDraw(commandBuffer.GetCommandBuffer(), 4, glyphInstanceCount, 0, 0);
	}


	Shader::VertexInput RendererFonts2::GetVertexInput(const uint32_t &binding)
	{
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);

		// The vertex input description.
		bindingDescriptions[0].binding = binding;
		bindingDescriptions[0].stride = sizeof(GlyphInstance);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);

		// Rectangle attribute.
		attributeDescriptions[0].binding = binding;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(GlyphInstance, rect);

		// Glyph Index attribute.
		attributeDescriptions[1].binding = binding;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32_UINT;
		attributeDescriptions[1].offset = offsetof(GlyphInstance, glyphIndex);

		// Sharpness attribute.
		attributeDescriptions[2].binding = binding;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(GlyphInstance, sharpness);

		// Colour attribute.
		attributeDescriptions[3].binding = binding;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(GlyphInstance, colour);

		return Shader::VertexInput(binding, bindingDescriptions, attributeDescriptions);
	}

	uint32_t RendererFonts2::AlignUint32(const uint32_t &value, const uint32_t &alignment)
	{
		return (value + alignment - 1) / alignment * alignment;
	}

	void RendererFonts2::LoadFont(const std::string &filename)
	{
		auto physicalDevice = Renderer::Get()->GetPhysicalDevice();

		FT_Library library;
		FT_CHECK(FT_Init_FreeType(&library));

		FT_Face face;
		FT_CHECK(FT_New_Face(library, filename.c_str(), 0, &face)); // TODO: FT_New_Memory_Face
		FT_CHECK(FT_Set_Char_Size(face, 0, 1000 * 64, 96, 96));

		uint32_t totalPoints = 0;
		uint32_t totalCells = 0;

		uint32_t glyphCount = face->num_glyphs;
		charmap = std::map<wchar_t, uint32_t>();
		glyphInfos = std::vector<HostGlyphInfo>(glyphCount);
		std::vector<Outline> outlines(glyphCount);

	//	Log::Out("Glyph Count: %i\n", glyphCount);

		FT_UInt glyphIndex;
		FT_ULong charcode = FT_Get_First_Char(face, &glyphIndex);
		uint32_t i = 0;

		while (glyphIndex != 0)
		{
			FT_CHECK(FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_HINTING));
		//	Log::Out("%i(%i) = %c\n", i, glyphIndex, charcode);

			charmap.emplace(charcode, i);
			HostGlyphInfo *hgi = &glyphInfos[i];
			Outline *o = &outlines[i];

			OutlineConvert(&face->glyph->outline, o);

			hgi->bbox = o->bbox;
			hgi->advance = face->glyph->metrics.horiAdvance / 64.0f;

			totalPoints += o->points.size();
			totalCells += o->cells.size();

			charcode = FT_Get_Next_Char(face, charcode, &glyphIndex);
			i++;
		}

		glyphInfoSize = sizeof(DeviceGlyphInfo) * glyphInfos.size();
		glyphCellsSize = sizeof(uint32_t) * totalCells;
		glyphPointsSize = sizeof(Vector2) * totalPoints;

		uint32_t alignment = physicalDevice->GetProperties().limits.minStorageBufferOffsetAlignment;
		glyphInfoOffset = 0;
		glyphCellsOffset = AlignUint32(glyphInfoSize, alignment);
		glyphPointsOffset = AlignUint32(glyphInfoSize + glyphCellsSize, alignment);

		glyphDataSize = glyphPointsOffset + glyphPointsSize;
		glyphData = std::make_unique<char[]>(glyphDataSize);

		auto deviceGlyphInfos = reinterpret_cast<DeviceGlyphInfo*>(glyphData.get() + glyphInfoOffset);
		auto cells = reinterpret_cast<uint32_t*>(glyphData.get() + glyphCellsOffset);
		auto points = reinterpret_cast<Vector2*>(glyphData.get() + glyphPointsOffset);

		uint32_t pointOffset = 0;
		uint32_t cellOffset = 0;

		for (uint32_t i = 0; i < glyphInfos.size(); i++)
		{
			Outline *o = &outlines[i];
			DeviceGlyphInfo *dgi = &deviceGlyphInfos[i];

			dgi->cellInfo.cellCountX = o->cellCountX;
			dgi->cellInfo.cellCountY = o->cellCountY;
			dgi->cellInfo.pointOffset = pointOffset;
			dgi->cellInfo.cellOffset = cellOffset;
			dgi->bbox = o->bbox;

			memcpy(cells + cellOffset, o->cells.data(), sizeof(uint32_t) * o->cells.size());
			memcpy(points + pointOffset, o->points.data(), sizeof(Vector2) * o->points.size());

		//	OutlineU16Points(o, &dgi->cbox, reinterpret_cast<PointU16 *>(reinterpret_cast<char *>(points) + pointOffset));

			pointOffset += o->points.size();
			cellOffset += o->cells.size();
		}

		assert(pointOffset == totalPoints);
		assert(cellOffset == totalCells);

		FT_CHECK(FT_Done_Face(face));
		FT_CHECK(FT_Done_FreeType(library));
	}

	uint32_t FindMemoryType(const uint32_t &typeBits, const VkMemoryPropertyFlags &flags)
	{
		auto physicalDevice = Renderer::Get()->GetPhysicalDevice();
		auto memoryProperties = physicalDevice->GetMemoryProperties();

		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if (typeBits & 1 << i)
			{
				VkMemoryPropertyFlags f = memoryProperties.memoryTypes[i].propertyFlags;

				if ((f & flags) == flags)
				{
					return i;
				}
			}
		}

		return std::numeric_limits<uint32_t>::max();
	}

	VkDeviceMemory AllocRequiredMemory(VkMemoryRequirements *req, const VkMemoryPropertyFlags &flags)
	{
		auto logicalDevice = Renderer::Get()->GetLogicalDevice();

		VkMemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = req->size;
		memoryAllocateInfo.memoryTypeIndex = FindMemoryType(req->memoryTypeBits, flags);

		VkDeviceMemory memory;
		Renderer::CheckVk(vkAllocateMemory(logicalDevice->GetLogicalDevice(), &memoryAllocateInfo, nullptr, &memory));
		return memory;
	}

	void CreateBufferWithMemory(VkBufferCreateInfo *ci, const VkMemoryPropertyFlags &flags,
	                            VkDeviceMemory *memory, VkBuffer *buffer)
	{
		auto logicalDevice = Renderer::Get()->GetLogicalDevice();

		Renderer::CheckVk(vkCreateBuffer(logicalDevice->GetLogicalDevice(), ci, nullptr, buffer));

		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(logicalDevice->GetLogicalDevice(), *buffer, &requirements);

		*memory = AllocRequiredMemory(&requirements, flags);
		Renderer::CheckVk(vkBindBufferMemory(logicalDevice->GetLogicalDevice(), *buffer, *memory, 0));
	}

	static void CopyBuffer(const VkBuffer &srcBuffer, const VkBuffer &dstBuffer, const VkDeviceSize &size)
	{
		CommandBuffer commandBuffer = CommandBuffer();
		VkBufferCopy copy = {0, 0, size};

		vkCmdCopyBuffer(commandBuffer.GetCommandBuffer(), srcBuffer, dstBuffer, 1, &copy);
		commandBuffer.End();
		commandBuffer.SubmitIdle();
	}

	void StageBuffer(const VkBuffer &buffer, const void *data, const std::size_t &size)
	{
		auto logicalDevice = Renderer::Get()->GetLogicalDevice();

		VkBufferCreateInfo stagingCreateInfo = {};
		stagingCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingCreateInfo.size = size;
		stagingCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
	
		CreateBufferWithMemory(&stagingCreateInfo, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		&stagingBufferMemory, &stagingBuffer);
	
		void *stagingBufferPtr;
		Renderer::CheckVk(vkMapMemory(logicalDevice->GetLogicalDevice(), stagingBufferMemory, 0, stagingCreateInfo.size, 0, &stagingBufferPtr));
	
		memcpy(stagingBufferPtr, data, size);
	
		vkUnmapMemory(logicalDevice->GetLogicalDevice(), stagingBufferMemory);
	
		CopyBuffer(stagingBuffer, buffer, size);
	
		vkDestroyBuffer(logicalDevice->GetLogicalDevice(), stagingBuffer, nullptr);
		vkFreeMemory(logicalDevice->GetLogicalDevice(), stagingBufferMemory, nullptr);
	}

	void RendererFonts2::CreateStorageBuffer()
	{
		VkBufferCreateInfo storageCreateInfo = {};
		storageCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		storageCreateInfo.size = glyphDataSize;
		storageCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		storageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		CreateBufferWithMemory(&storageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&storageBufferMemory, &storageBuffer);

		StageBuffer(storageBuffer, glyphData.get(), storageCreateInfo.size);

		//	m_storageGlyphs = std::make_unique<StorageBuffer>(glyphDataSize);
	//	m_storageGlyphs->Update(glyphData.get());
		glyphData = nullptr;
	}

	void RendererFonts2::CreateInstanceBuffer()
	{
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance);
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		CreateBufferWithMemory(&bufferCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&instanceBufferMemory, &instanceBuffer);

		VkBufferCreateInfo stagingCreateInfo = {};
		stagingCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingCreateInfo.size = bufferCreateInfo.size;
		stagingCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		CreateBufferWithMemory(&stagingCreateInfo, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&instanceStagingBufferMemory, &instanceStagingBuffer);
	}

	void RendererFonts2::CreateDescriptorSet()
	{
		auto logicalDevice = Renderer::Get()->GetLogicalDevice();

		VkDescriptorSetAllocateInfo setAllocateInfo = {};
		setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		setAllocateInfo.descriptorPool = m_pipeline.GetDescriptorPool();
		setAllocateInfo.descriptorSetCount = 1;
		setAllocateInfo.pSetLayouts = &m_pipeline.GetDescriptorSetLayout();

		Renderer::CheckVk(vkAllocateDescriptorSets(logicalDevice->GetLogicalDevice(), &setAllocateInfo, &descriptorSet));

		VkDescriptorBufferInfo glyphInfo = {};
		glyphInfo.buffer = storageBuffer;
		glyphInfo.offset = glyphInfoOffset;
		glyphInfo.range = glyphInfoSize;

		VkDescriptorBufferInfo cellsInfo = {};
		cellsInfo.buffer = storageBuffer;
		cellsInfo.offset = glyphCellsOffset;
		cellsInfo.range = glyphCellsSize;

		VkDescriptorBufferInfo pointsInfo = {};
		pointsInfo.buffer = storageBuffer;
		pointsInfo.offset = glyphPointsOffset;
		pointsInfo.range = glyphPointsSize;

		std::array<VkWriteDescriptorSet, 3> writes = {};

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptorSet;
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &glyphInfo;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptorSet;
		writes[1].dstBinding = 1;
		writes[1].dstArrayElement = 0;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[1].pBufferInfo = &cellsInfo;

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = descriptorSet;
		writes[2].dstBinding = 2;
		writes[2].dstArrayElement = 0;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[2].pBufferInfo = &pointsInfo;

		vkUpdateDescriptorSets(logicalDevice->GetLogicalDevice(), writes.size(), writes.data(), 0, nullptr);
	}

	void RendererFonts2::AppendText(const float &x, const float &y, const float &scale, const std::wstring &text, const Colour &colour)
	{
		Vector2 extent = Window::Get()->GetDimensions();
		float localX = x;

		for (const auto &c : text)
		{
			if (glyphInstanceCount >= MAX_VISIBLE_GLYPHS)
			{
				break;
			}

			uint32_t glyphIndex = charmap[c];
			HostGlyphInfo *gi = &glyphInfos[glyphIndex];
			GlyphInstance *inst = &glyphInstances[glyphInstanceCount];

			inst->rect.minX = (localX + gi->bbox.minX * scale) / (extent.m_x / 2.0f) - 1.0f;
			inst->rect.minY = (y - gi->bbox.minY * scale) / (extent.m_y / 2.0f) - 1.0f;
			inst->rect.maxX = (localX + gi->bbox.maxX * scale) / (extent.m_x / 2.0f) - 1.0f;
			inst->rect.maxY = (y - gi->bbox.maxY * scale) / (extent.m_y / 2.0f) - 1.0f;

			if (inst->rect.minX <= 1.0f && inst->rect.maxX >= -1.0f &&
			    inst->rect.maxY <= 1.0f && inst->rect.minY >= -1.0f)
			{
				inst->glyphIndex = glyphIndex;
				inst->sharpness = scale;
				inst->colour = colour;

				glyphInstanceCount++;
			}

			localX += gi->advance * scale;
		}
	}

	void RendererFonts2::AppendText(const float &x, const float &y, const float &scale, const std::string &text, const Colour &colour)
	{
		std::wstring temp(text.begin(), text.end());
		AppendText(x, y, scale, temp, colour);
	}
}
