#include "Cell.hpp"

#include <assert.h>

namespace acid
{
	uint32_t Cell::AddRange(const uint32_t &cell, const uint32_t &from, const uint32_t &to) const
	{
		assert(from % 2 == 0 && to % 2 == 0);

		uint32_t rangeCell = cell;
		uint32_t rangeFrom = from;
		uint32_t rangeTo = to;

		rangeFrom /= 2;
		rangeTo /= 2;

		if (rangeFrom >= std::numeric_limits<uint8_t>::max())
		{
			return 0;
		}

		if (rangeTo >= std::numeric_limits<uint8_t>::max())
		{
			return 0;
		}

		uint32_t length = rangeTo - rangeFrom;

		if (length <= 3 && (cell & 0x03) == 0)
		{
			rangeCell |= rangeFrom << 8;
			rangeCell |= length;
			return rangeCell;
		}

		if (length > 7)
		{
			return 0;
		}

		if ((rangeCell & 0x1C) == 0)
		{
			rangeCell |= rangeFrom << 16;
			rangeCell |= length << 2;
			return rangeCell;
		}

		if ((rangeCell & 0xE0) == 0)
		{
			rangeCell |= rangeFrom << 24;
			rangeCell |= length << 5;
			return rangeCell;
		}

		return 0;
	}

	bool Cell::AddBezier(const Outline &o, const Outline &u, const uint32_t &i, const uint32_t &j,
	                     const uint32_t &contourIndex)
	{
		bool ret = true;
		uint32_t ucontourBegin = u.contours[contourIndex].begin;

		if (to != std::numeric_limits<uint32_t>::max() && to != j)
		{
			assert(to < j);

			if (from == ucontourBegin)
			{
				assert(to % 2 == 0);
				assert(from % 2 == 0);

				startLength = (to - from) / 2;
			}
			else
			{
				value = AddRange(value, from, to);

				if (!value)
				{
					ret = false;
				}
			}

			from = j;
		}
		else
		{
			if (from == std::numeric_limits<uint32_t>::max())
			{
				from = j;
			}
		}

		to = j + 2;
		return ret;
	}

	bool Cell::FinishContour(const Outline &o, const Outline &u, const uint32_t &contourIndex, uint32_t &maxStartLength)
	{
		bool ret = true;
		uint32_t ucontour_begin = u.contours[contourIndex].begin;
		uint32_t ucontour_end = u.contours[contourIndex].end;

		if (to < ucontour_end)
		{
			value = AddRange(value, from, to);

			if (!value)
			{
				ret = false;
			}

			from = std::numeric_limits<uint32_t>::max();
			to = std::numeric_limits<uint32_t>::max();
		}

		assert(to == std::numeric_limits<uint32_t>::max() || to == ucontour_end);
		to = std::numeric_limits<uint32_t>::max();

		if (from != std::numeric_limits<uint32_t>::max() && startLength != 0)
		{
			value = AddRange(value, from, ucontour_end + startLength * 2);

			if (!value)
			{
				ret = false;
			}

			maxStartLength = std::max(maxStartLength, startLength);
			from = std::numeric_limits<uint32_t>::max();
			startLength = 0;
		}

		if (from != std::numeric_limits<uint32_t>::max())
		{
			value = AddRange(value, from, ucontour_end);

			if (!value)
			{
				ret = false;
			}

			from = std::numeric_limits<uint32_t>::max();
		}

		if (startLength != 0)
		{
			value = AddRange(value, ucontour_begin, ucontour_begin + startLength * 2);

			if (!value)
			{
				ret = false;
			}

			startLength = 0;
		}

		assert(from == std::numeric_limits<uint32_t>::max() && to == std::numeric_limits<uint32_t>::max());
		return ret;
	}

	bool Cell::CellsAddBezier(const Outline &o, const Outline &u, const uint32_t &i, const uint32_t &j,
	                          const uint32_t &contourIndex, Cell *cells)
	{
		Rect bezier_bbox = bezier2_bbox(&o.points[i]);

		float outline_bbox_w = o.bbox.maxX - o.bbox.minX;
		float outline_bbox_h = o.bbox.maxY - o.bbox.minY;

		auto min_x = static_cast<uint32_t>((bezier_bbox.minX - o.bbox.minX) / outline_bbox_w * o.cellCountX);
		auto min_y = static_cast<uint32_t>((bezier_bbox.minY - o.bbox.minY) / outline_bbox_h * o.cellCountY);
		auto max_x = static_cast<uint32_t>((bezier_bbox.maxX - o.bbox.minX) / outline_bbox_w * o.cellCountX);
		auto max_y = static_cast<uint32_t>((bezier_bbox.maxY - o.bbox.minY) / outline_bbox_h * o.cellCountY);

		if (max_x >= o.cellCountX)
		{
			max_x = o.cellCountX - 1;
		}

		if (max_y >= o.cellCountY)
		{
			max_y = o.cellCountY - 1;
		}

		bool ret = true;

		for (uint32_t y = min_y; y <= max_y; y++)
		{
			for (uint32_t x = min_x; x <= max_x; x++)
			{
				Cell &cell = cells[y * o.cellCountX + x];

				if (bbox_bezier2_intersect(cell.bbox, &o.points[i]))
				{
					ret &= cell.AddBezier(o, u, i, j, contourIndex);
				}
			}
		}

		return ret;
	}

	bool Cell::CellsFinishContour(const Outline &o, const Outline &u, const uint32_t &contourIndex, Cell *cells,
	                              uint32_t &maxStartLength)
	{
		bool ret = true;

		for (uint32_t y = 0; y < o.cellCountY; y++)
		{
			for (uint32_t x = 0; x < o.cellCountX; x++)
			{
				Cell &cell = cells[y * o.cellCountX + x];
				ret &= cell.FinishContour(o, u, contourIndex, maxStartLength);
			}
		}

		return ret;
	}

	void Cell::CellsSetFilled(const Outline &u, Cell *cells, const uint32_t &filledCell)
	{
		for (uint32_t y = 0; y < u.cellCountY; y++)
		{
			for (uint32_t x = 0; x < u.cellCountX; x++)
			{
				uint32_t i = y * u.cellCountX + x;
				Cell *cell = &cells[i];

				if (cell->value == 0 && u.is_cell_filled(cell->bbox))
				{
					cell->value = filledCell;
				}
			}
		}
	}
}
