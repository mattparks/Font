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
		m_pipeline(PipelineGraphics(pipelineStage, {"Shaders/Fonts2/Font.vert", "Shaders/Fonts2/Font.frag"}, {RendererFonts2::GetVertexInput()},
			PipelineGraphics::Mode::Polygon, PipelineGraphics::Depth::None, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false, {})),
		m_descriptorSet(DescriptorsHandler()),
		m_storageGlyphs(nullptr),
		m_instanceBuffer(InstanceBuffer(MAX_VISIBLE_GLYPHS * sizeof(GlyphInstance))),
		m_canvasScale(3.0f),
		m_targetCanvasScale(m_canvasScale)
	{
		Mouse::Get()->GetOnScroll() += [this](float offsetX, float offsetY)
		{
			m_targetCanvasScale *= std::pow(1.3f, offsetY);
		};

	//	std::string filename = "Fonts/Alice-Regular.ttf";
	//	std::string filename = "Fonts/Jokerman-Regular.ttf";
	//	std::string filename = "Fonts/marediv.ttf";
		std::string filename = "Fonts/Lobster-Regular.ttf";
	//	std::string filename = "Fonts/LobsterTwo-Bold.ttf";
	//	std::string filename = "Fonts/LobsterTwo-BoldItalic.ttf";
	//	std::string filename = "Fonts/LobsterTwo-Italic.ttf";
	//	std::string filename = "Fonts/LobsterTwo-Regular.ttf";
	//	std::string filename = "Fonts/OpenSans-Bold.ttf";
	//	std::string filename = "Fonts/OpenSans-BoldItalic.ttf";
	//	std::string filename = "Fonts/OpenSans-ExtraBold.ttf";
	//	std::string filename = "Fonts/OpenSans-ExtraBoldItalic.ttf";
	//	std::string filename = "Fonts/OpenSans-Italic.ttf";
	//	std::string filename = "Fonts/OpenSans-Light.ttf";
	//	std::string filename = "Fonts/OpenSans-LightItalic.ttf";
	//	std::string filename = "Fonts/OpenSans-Regular.ttf";
	//	std::string filename = "Fonts/OpenSans-SemiBold.ttf";
	//	std::string filename = "Fonts/OpenSans-SemiBoldItalic.ttf";
	//	std::string filename = "Fonts/Roboto-Black.ttf";
	//	std::string filename = "Fonts/Roboto-BlackItalic.ttf";
	//	std::string filename = "Fonts/Roboto-Bold.ttf";
	//	std::string filename = "Fonts/Roboto-BoldItalic.ttf";
	//	std::string filename = "Fonts/Roboto-Italic.ttf";
	//	std::string filename = "Fonts/Roboto-Light.ttf";
	//	std::string filename = "Fonts/Roboto-LightItalic.ttf";
	//	std::string filename = "Fonts/Roboto-Medium.ttf";
	//	std::string filename = "Fonts/Roboto-MediumItalic.ttf";
	//	std::string filename = "Fonts/Roboto-Regular.ttf";
	//	std::string filename = "Fonts/Roboto-Thin.ttf";
	//	std::string filename = "Fonts/Roboto-ThinItalic.ttf";
		LoadFont(filename);
	}

	void RendererFonts2::Render(const CommandBuffer &commandBuffer)
	{
		m_mousePos = Mouse::Get()->GetPosition() * Window::Get()->GetDimensions();
		Vector2 mouseDelta = m_oldMousePos - m_mousePos;

		if (Mouse::Get()->GetButton(MouseButton::Left) != InputAction::Release)
		{
			mouseDelta *= 1.0f / m_canvasScale;
			m_canvasOffset += mouseDelta;
		}

		m_oldMousePos = m_mousePos;

		if (m_canvasScale != m_targetCanvasScale)
		{
			Vector2 oldSize = Window::Get()->GetDimensions() * (1.0f / m_canvasScale);
			m_canvasScale = Maths::Lerp(m_canvasScale, m_targetCanvasScale, 30.0f * Engine::Get()->GetDeltaRender().AsSeconds());
			Vector2 newSize = Window::Get()->GetDimensions() * (1.0f / m_canvasScale);

			Vector2 tmp = oldSize - newSize;
			tmp *= m_mousePos / Window::Get()->GetDimensions();
			m_canvasOffset += tmp;
		}

		m_glyphInstanceCount = 0;
		m_instanceBuffer.Map(reinterpret_cast<void **>(&m_glyphInstances));

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
			AppendText(m_canvasScale * (10.0f - m_canvasOffset.m_x),
				m_canvasScale * (30.0f - m_canvasOffset.m_y + i * 30.0f),
				0.02f * m_canvasScale, lines[i], Colour::Black);
		}

		AppendText(5.0f, 25.0f, 0.02f, "Frame Time: " + String::To(1000.0f / Engine::Get()->GetFps()) + "ms", Colour::Red);
		AppendText(5.0f, 55.0f, 0.02f, "Fps: " + String::To<uint32_t>(Engine::Get()->GetFps()), Colour::Green);
		AppendText(5.0f, 85.0f, 0.02f, "Ups: " + String::To<uint32_t>(Engine::Get()->GetUps()), Colour::Blue);

		m_instanceBuffer.Unmap();

		m_pipeline.BindPipeline(commandBuffer);

		// Updates descriptors.
		m_descriptorSet.Push("GlyphBuffer", *m_storageGlyphs, OffsetSize(m_glyphInfoOffset, m_glyphInfoSize));
		m_descriptorSet.Push("CellBuffer", *m_storageGlyphs, OffsetSize(m_glyphCellsOffset, m_glyphCellsSize));
		m_descriptorSet.Push("PointBuffer", *m_storageGlyphs, OffsetSize(m_glyphPointsOffset, m_glyphPointsSize));
		bool updateSuccess = m_descriptorSet.Update(m_pipeline);

		if (!updateSuccess)
		{
			return;
		}

		// Draws the object.
		m_descriptorSet.BindDescriptor(commandBuffer, m_pipeline);

		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer.GetCommandBuffer(), 0, 1, &m_instanceBuffer.GetBuffer(), offsets);
		vkCmdDraw(commandBuffer.GetCommandBuffer(), 4, m_glyphInstanceCount, 0, 0);
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

		auto fileLoaded = Files::Read(filename);

		if (!fileLoaded)
		{
			Log::Error("Font could not be loaded: '%s'\n", filename.c_str());
			return;
		}

		FT_Library library;
		FT_CHECK(FT_Init_FreeType(&library));

		FT_Face face;
		FT_CHECK(FT_New_Memory_Face(library, (FT_Byte*)fileLoaded->data(), (FT_Long)fileLoaded->size(), 0, &face));
		FT_CHECK(FT_Set_Char_Size(face, 0, 1000 * 64, 96, 96));

		uint32_t totalPoints = 0;
		uint32_t totalCells = 0;

		uint32_t glyphCount = face->num_glyphs;
		m_charmap = std::map<wchar_t, uint32_t>();
		m_glyphInfos = std::vector<HostGlyphInfo>(glyphCount);
		std::vector<Outline> outlines(glyphCount);

		Log::Out("Glyph Count: %i\n", glyphCount);

		FT_UInt glyphIndex;
		FT_ULong charcode = FT_Get_First_Char(face, &glyphIndex);
		uint32_t i = 0;

		while (glyphIndex != 0)
		{
			FT_CHECK(FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_HINTING));

			m_charmap.emplace(charcode, i);
			HostGlyphInfo *hgi = &m_glyphInfos[i];
			Outline *o = &outlines[i];

			OutlineConvert(&face->glyph->outline, o);

			hgi->bbox = o->bbox;
			hgi->advance = face->glyph->metrics.horiAdvance / 64.0f;

			totalPoints += o->points.size();
			totalCells += o->cells.size();

			charcode = FT_Get_Next_Char(face, charcode, &glyphIndex);
			i++;
		}

		m_glyphInfoSize = sizeof(DeviceGlyphInfo) * m_glyphInfos.size();
		m_glyphCellsSize = sizeof(uint32_t) * totalCells;
		m_glyphPointsSize = sizeof(Vector2) * totalPoints;

		uint32_t alignment = physicalDevice->GetProperties().limits.minStorageBufferOffsetAlignment;
		m_glyphInfoOffset = 0;
		m_glyphCellsOffset = AlignUint32(m_glyphInfoSize, alignment);
		m_glyphPointsOffset = AlignUint32(m_glyphInfoSize + m_glyphCellsSize, alignment);

		m_glyphDataSize = m_glyphPointsOffset + m_glyphPointsSize;
		m_storageGlyphs = std::make_unique<StorageBuffer>(m_glyphDataSize);

		char *glyphData;
		m_storageGlyphs->Map(reinterpret_cast<void **>(&glyphData));

		auto deviceGlyphInfos = reinterpret_cast<DeviceGlyphInfo*>(glyphData + m_glyphInfoOffset);
		auto cells = reinterpret_cast<uint32_t*>(glyphData + m_glyphCellsOffset);
		auto points = reinterpret_cast<Vector2*>(glyphData + m_glyphPointsOffset);

		uint32_t pointOffset = 0;
		uint32_t cellOffset = 0;

		for (uint32_t j = 0; j < m_glyphInfos.size(); j++)
		{
			Outline *o = &outlines[j];
			DeviceGlyphInfo *dgi = &deviceGlyphInfos[j];

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

		m_storageGlyphs->Unmap();

		assert(pointOffset == totalPoints);
		assert(cellOffset == totalCells);

		FT_CHECK(FT_Done_Face(face));
		FT_CHECK(FT_Done_FreeType(library));
	}

	void RendererFonts2::AppendText(const float &x, const float &y, const float &scale, const std::wstring &text, const Colour &colour)
	{
		Vector2 extent = Window::Get()->GetDimensions();
		float localX = x;

		for (const auto &c : text)
		{
			if (m_glyphInstanceCount >= MAX_VISIBLE_GLYPHS)
			{
				break;
			}

			uint32_t glyphIndex = m_charmap[c];
			HostGlyphInfo *gi = &m_glyphInfos[glyphIndex];
			GlyphInstance *inst = &m_glyphInstances[m_glyphInstanceCount];

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

				m_glyphInstanceCount++;
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
