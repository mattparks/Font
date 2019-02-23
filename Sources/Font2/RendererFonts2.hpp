#pragma once

#include <Models/Model.hpp>
#include <Maths/Colour.hpp>
#include <Renderer/RenderPipeline.hpp>
#include <Renderer/Buffers/InstanceBuffer.hpp>
#include <Renderer/Pipelines/PipelineGraphics.hpp>
#include <Renderer/Handlers/DescriptorsHandler.hpp>
#include <Renderer/Handlers/StorageHandler.hpp>
#include <Renderer/Handlers/UniformHandler.hpp>
#include "Outline.hpp"

namespace acid
{
	class RendererFonts2 :
		public RenderPipeline
	{
	public:
		explicit RendererFonts2(const Pipeline::Stage &pipelineStage);

		void Render(const CommandBuffer &commandBuffer) override;
	private:
		struct CellInfo
		{
			uint32_t pointOffset;
			uint32_t cellOffset;
			uint32_t cellCountX;
			uint32_t cellCountY;
		};

		struct GlyphInstance
		{
			Rect rect;
			uint32_t glyphIndex;
			float sharpness;
			Colour colour;
		};

		struct HostGlyphInfo
		{
			Rect bbox;
			float advance;
		};

		struct DeviceGlyphInfo
		{
			Rect bbox;
		//	Rect cbox;
			CellInfo cellInfo;
		};

		static Shader::VertexInput GetVertexInput(const uint32_t &binding = 0);
		static uint32_t AlignUint32(const uint32_t &value, const uint32_t &alignment);
		void LoadFont(const std::string &filename);
		void CreateStorageBuffer();
		void CreateInstanceBuffer();
		void CreateDescriptorSet();
		void AppendText(const float &x, const float &y, const float &scale, const std::wstring &text, const Colour &colour);
		void AppendText(const float &x, const float &y, const float &scale, const std::string &text, const Colour &colour);

		PipelineGraphics m_pipeline;
	//	DescriptorsHandler m_descriptorSet;
	//	std::unique_ptr<StorageBuffer> m_storageGlyphs;
	//	InstanceBuffer m_instanceBuffer;

		std::map<wchar_t, uint32_t> charmap;
		std::vector<HostGlyphInfo> glyphInfos;

		std::unique_ptr<char[]> glyphData;
		uint32_t glyphDataSize;
		uint32_t glyphInfoSize;
		uint32_t glyphCellsSize;
		uint32_t glyphPointsSize;
		uint32_t glyphInfoOffset;
		uint32_t glyphCellsOffset;
		uint32_t glyphPointsOffset;

		GlyphInstance *glyphInstances;
		uint32_t glyphInstanceCount;

		VkDeviceMemory storageBufferMemory;
		VkBuffer storageBuffer;

		VkDeviceMemory instanceBufferMemory;
		VkBuffer instanceBuffer;
		VkDeviceMemory instanceStagingBufferMemory;
		VkBuffer instanceStagingBuffer;

		VkDescriptorSet descriptorSet;
	};
}
