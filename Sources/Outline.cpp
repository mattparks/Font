#include "Outline.hpp"

#include <assert.h>
#include <string.h>
#include <vector>
#include "Geometry.hpp"
#include "Cell.hpp"

namespace acid
{
	void Outline::add_outline_point(const Vector2 &point)
	{
		if (pointCapacity == pointCount)
		{
			dyn_array_grow((void **) &points, &pointCapacity, sizeof(Vector2));
		}

		points[pointCount] = point;
		pointCount++;
	}

	void Outline::add_outline_contour(ContourRange *range)
	{
		if (contourCapacity == contourCount)
		{
			dyn_array_grow(&contours, &contourCapacity, sizeof(ContourRange));
		}

		contours[contourCount] = *range;
		contourCount++;
	}

	void Outline::outline_add_odd_point()
	{
		if (pointCount % 2 != 0)
		{
			Vector2 p = {bbox.minX, bbox.minY};
			add_outline_point(p);
		}
	}

	static Vector2 convert_point(const FT_Vector *v)
	{
		return Vector2(
			static_cast<float>(v->x) / 64.0f,
			static_cast<float>(v->y) / 64.0f
		);
	}

	static int move_to_func(const FT_Vector *to, Outline &o)
	{
		Vector2 p = Vector2();

		if (o.contourCount > 0)
		{
			o.contours[o.contourCount - 1].end = o.pointCount - 1;
			o.add_outline_point(p);
		}

		assert(o.pointCount % 2 == 0);

		ContourRange range = {o.pointCount, std::numeric_limits<uint32_t>::max()};
		o.add_outline_contour(&range);

		p = convert_point(to);
		o.add_outline_point(p);
		return 0;
	}

	static int line_to_func(const FT_Vector *to, Outline &o)
	{
		uint32_t last = o.pointCount - 1;

		Vector2 to_p;
		to_p = convert_point(to);
		Vector2 p = o.points[last].Lerp(to_p, 0.5f);
		o.add_outline_point(p);
		o.add_outline_point(to_p);
		return 0;
	}

	static int conic_to_func(const FT_Vector *control, const FT_Vector *to, Outline &o)
	{
		Vector2 p;
		p = convert_point(control);
		o.add_outline_point(p);

		p = convert_point(to);
		o.add_outline_point(p);
		return 0;
	}

	static int cubic_to_func(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, Outline &o)
	{
		return line_to_func(to, o);
	}

	void Outline::outline_decompose(FT_Outline *outline)
	{
		memset(this, 0, sizeof(Outline));

		FT_BBox outline_bbox;
		FT_CHECK(FT_Outline_Get_BBox(outline, &outline_bbox));

		bbox.minX = static_cast<float>(outline_bbox.xMin) / 64.0f;
		bbox.minY = static_cast<float>(outline_bbox.yMin) / 64.0f;
		bbox.maxX = static_cast<float>(outline_bbox.xMax) / 64.0f;
		bbox.maxY = static_cast<float>(outline_bbox.yMax) / 64.0f;

		FT_Outline_Funcs funcs = {};
		funcs.move_to = reinterpret_cast<FT_Outline_MoveToFunc>(move_to_func);
		funcs.line_to = reinterpret_cast<FT_Outline_LineToFunc>(line_to_func);
		funcs.conic_to = reinterpret_cast<FT_Outline_ConicToFunc>(conic_to_func);
		funcs.cubic_to = reinterpret_cast<FT_Outline_CubicToFunc>(cubic_to_func);

		FT_CHECK(FT_Outline_Decompose(outline, &funcs, this));

		if (contourCount > 0)
		{
			contours[contourCount - 1].end = pointCount - 1;
		}
	}

	bool Outline::is_cell_filled(const Rect &bbox) const
	{
		Vector2 p = {
			(bbox.maxX + bbox.minX) / 2.0f,
			(bbox.maxY + bbox.minY) / 2.0f,
		};

		float mindist = std::numeric_limits<float>::max();
		float v = std::numeric_limits<float>::max();
		uint32_t j = std::numeric_limits<uint32_t>::max();

		for (uint32_t contour_index = 0; contour_index < contourCount; contour_index++)
		{
			uint32_t contour_begin = contours[contour_index].begin;
			uint32_t contour_end = contours[contour_index].end;

			for (uint32_t i = contour_begin; i < contour_end; i += 2)
			{
				Vector2 p0 = points[i];
				Vector2 p1 = points[i + 1];
				Vector2 p2 = points[i + 2];

				float t = line_calculate_t(p0, p2, p);

				Vector2 p02 = p0.Lerp(p2, t);

				float udist = p02.Distance(p);

				if (udist < mindist + 0.0001f)
				{
					float d = line_signed_distance(p0, p2, p);

					if (udist >= mindist && i > contour_begin)
					{
						float lastd = i == contour_end - 2 && j == contour_begin
						              ? line_signed_distance(p0, p2, points[contour_begin + 2])
						              : line_signed_distance(p0, p2, points[i - 2]);

						if (lastd < 0.0f)
						{
							v = std::max(d, v);
						}
						else
						{
							v = std::min(d, v);
						}
					}
					else
					{
						v = d;
					}

					mindist = std::min(mindist, udist);
					j = i;
				}
			}
		}

		return v > 0.0f;
	}

	void Outline::copy_wipcell_values(Cell *wipCells)
	{
		cells = (uint32_t *) malloc(sizeof(uint32_t) * cellCountX * cellCountY);

		for (uint32_t y = 0; y < cellCountY; y++)
		{
			for (uint32_t x = 0; x < cellCountX; x++)
			{
				uint32_t i = y * cellCountX + x;
				cells[i] = wipCells[i].value;
			}
		}
	}

	void Outline::init_wipcells(Cell *wipCells)
	{
		float w = bbox.maxX - bbox.minX;
		float h = bbox.maxY - bbox.minY;

		for (uint32_t y = 0; y < cellCountY; y++)
		{
			for (uint32_t x = 0; x < cellCountX; x++)
			{
				Rect cbox = {
					bbox.minX + ((float) x / cellCountX) * w,
					bbox.minY + ((float) y / cellCountY) * h,
					bbox.minX + ((float) (x + 1) / cellCountX) * w,
					bbox.minY + ((float) (y + 1) / cellCountY) * h,
				};

				uint32_t i = y * cellCountX + x;
				wipCells[i].bbox = cbox;
				wipCells[i].from = std::numeric_limits<uint32_t>::max();
				wipCells[i].to = std::numeric_limits<uint32_t>::max();
				wipCells[i].value = 0;
				wipCells[i].startLength = 0;
			}
		}
	}

	uint32_t Outline::outline_add_filled_line()
	{
		outline_add_odd_point();

		uint32_t i = pointCount;
		float y = bbox.maxY + 1000.0f;
		Vector2 f0 = {bbox.minX, y};
		Vector2 f1 = {bbox.minX + 10.0f, y};
		Vector2 f2 = {bbox.minX + 20.0f, y};
		add_outline_point(f0);
		add_outline_point(f1);
		add_outline_point(f2);

		return i;
	}

	static uint32_t make_cell_from_single_edge(const uint32_t &e)
	{
		assert(e % 2 == 0);
		return e << 7 | 1;
	}

	bool Outline::try_to_fit_in_cell_count()
	{
		bool ret = true;

		auto cells = std::vector<Cell>(cellCountX * cellCountY);
		init_wipcells(cells.data());

		Outline u = {};
		u.bbox = bbox;
		u.cellCountX = cellCountX;
		u.cellCountY = cellCountY;

		for (uint32_t contour_index = 0; contour_index < contourCount; contour_index++)
		{
			uint32_t contour_begin = contours[contour_index].begin;
			uint32_t contour_end = contours[contour_index].end;

			u.outline_add_odd_point();

			ContourRange urange = {u.pointCount, u.pointCount + contour_end - contour_begin};
			u.add_outline_contour(&urange);

			for (uint32_t i = contour_begin; i < contour_end; i += 2)
			{
				Vector2 p0 = points[i];
				Vector2 p1 = points[i + 1];

				uint32_t j = u.pointCount;
				u.add_outline_point(p0);
				u.add_outline_point(p1);

				ret &= Cell::CellsAddBezier(*this, u, i, j, contour_index, cells.data());
			}

			uint32_t maxStartLength = 0;
			ret &= Cell::CellsFinishContour(*this, u, contour_index, cells.data(), maxStartLength);

			uint32_t continuation_end = contour_begin + maxStartLength * 2;

			for (uint32_t i = contour_begin; i < continuation_end; i += 2)
			{
				u.add_outline_point(points[i]);
				u.add_outline_point(points[i + 1]);
			}

			Vector2 plast = points[continuation_end];
			u.add_outline_point(plast);
		}

		if (!ret)
		{
			u.outline_destroy();
			return ret;
		}

		uint32_t filled_line = u.outline_add_filled_line();
		uint32_t filled_cell = make_cell_from_single_edge(filled_line);
		Cell::CellsSetFilled(u, cells.data(), filled_cell);

		u.copy_wipcell_values(cells.data());

		outline_destroy();
		*this = u;
		return ret;
	}

	static uint32_t uint32_to_pow2(const uint32_t &v)
	{
		uint32_t r = v;
		r--;
		r |= r >> 1;
		r |= r >> 2;
		r |= r >> 4;
		r |= r >> 8;
		r |= r >> 16;
		r++;
		return r;
	}

	void Outline::outline_make_cells()
	{
		if (pointCount > OUTLINE_MAX_POINTS)
		{
			return;
		}

		float w = bbox.maxX - bbox.minX;
		float h = bbox.maxY - bbox.minY;

		uint32_t c = uint32_to_pow2((uint32_t) sqrtf(pointCount * 0.75f));

		cellCountX = c;
		cellCountY = c;

		if (h > w * 1.8f)
		{
			cellCountX /= 2;
		}

		if (w > h * 1.8f)
		{
			cellCountY /= 2;
		}

		while (true)
		{
			if (try_to_fit_in_cell_count())
			{
				break;
			}

			if (cellCountX > 64 || cellCountY > 64)
			{
				cellCountX = 0;
				cellCountY = 0;
				return;
			}

			if (cellCountX == cellCountY)
			{
				if (w > h)
				{
					cellCountX *= 2;
				}
				else
				{
					cellCountY *= 2;
				}
			}
			else
			{
				if (cellCountX < cellCountY)
				{
					cellCountX *= 2;
				}
				else
				{
					cellCountY *= 2;
				}
			}
		}
	}

	void Outline::outline_subdivide()
	{
		Outline u = {};
		u.bbox = bbox;

		for (uint32_t contour_index = 0; contour_index < contourCount; contour_index++)
		{
			uint32_t contour_begin = contours[contour_index].begin;
			uint32_t contour_end = contours[contour_index].end;

			u.outline_add_odd_point();

			ContourRange urange = {u.pointCount, std::numeric_limits<uint32_t>::max()};
			u.add_outline_contour(&urange);

			for (uint32_t i = contour_begin; i < contour_end; i += 2)
			{
				Vector2 p0 = points[i];

				Vector2 newp[3];
				bezier2_split_3p(newp, &points[i], 0.5f);

				u.add_outline_point(p0);
				u.add_outline_point(newp[0]);
				u.add_outline_point(newp[1]);
				u.add_outline_point(newp[2]);
			}

			u.contours[contour_index].end = u.pointCount;
			u.add_outline_point(points[contour_end]);
		}

		outline_destroy();
		*this = u;
	}

	void Outline::outline_fix_thin_lines()
	{
		Outline u = {};
		u.bbox = bbox;

		for (uint32_t contour_index = 0; contour_index < contourCount; contour_index++)
		{
			uint32_t contour_begin = contours[contour_index].begin;
			uint32_t contour_end = contours[contour_index].end;

			u.outline_add_odd_point();

			ContourRange urange = {u.pointCount, std::numeric_limits<uint32_t>::max()};
			u.add_outline_contour(&urange);

			for (uint32_t i = contour_begin; i < contour_end; i += 2)
			{
				Vector2 p0 = points[i];
				Vector2 p1 = points[i + 1];
				Vector2 p2 = points[i + 2];

				Vector2 mid = p0.Lerp(p2, 0.5f);
				Vector2 midp1 = p1 - mid;

				Vector2 bezier[] = {
					{p0.m_x, p0.m_y},
					{p1.m_x, p1.m_y},
					{p2.m_x, p2.m_y}
				};

				bezier[1] += midp1;
				bool subdivide = false;

				for (uint32_t j = contour_begin; j < contour_end; j += 2)
				{
					if (i == contour_begin && j == contour_end - 2)
					{
						continue;
					}

					if (i == contour_end - 2 && j == contour_begin)
					{
						continue;
					}

					if (j + 2 >= i && j <= i + 2)
					{
						continue;
					}

					Vector2 q0 = points[j];
					Vector2 q2 = points[j + 2];

					if (bezier2_line_is_intersecting(bezier, q0, q2))
					{
						subdivide = true;
					}
				}

				if (subdivide)
				{
					Vector2 newp[3];
					bezier2_split_3p(newp, &points[i], 0.5f);

					u.add_outline_point(p0);
					u.add_outline_point(newp[0]);
					u.add_outline_point(newp[1]);
					u.add_outline_point(newp[2]);
				}
				else
				{
					u.add_outline_point(p0);
					u.add_outline_point(p1);
				}
			}

			u.contours[contour_index].end = u.pointCount;
			u.add_outline_point(points[contour_end]);
		}

		outline_destroy();
		*this = u;
	}

	void Outline::outline_convert(FT_Outline *outline)
	{
		outline_decompose(outline);
		outline_fix_thin_lines();
		outline_make_cells();
	}

	void Outline::outline_destroy()
	{
		if (contours)
		{
			free(contours);
		}

		if (points)
		{
			free(points);
		}

		if (cells)
		{
			free(cells);
		}

		memset(this, 0, sizeof(Outline));
	}

	Rect Outline::outline_cbox() const
	{
		if (pointCount == 0)
		{
			return {};
		}

		Rect cbox = {};
		cbox.minX = points[0].m_x;
		cbox.minY = points[0].m_y;
		cbox.maxX = points[0].m_x;
		cbox.maxY = points[0].m_y;

		for (uint32_t i = 1; i < pointCount; i++)
		{
			float x = points[i].m_x;
			float y = points[i].m_y;

			cbox.minX = std::min(cbox.minX, x);
			cbox.minY = std::min(cbox.minY, y);
			cbox.maxX = std::max(cbox.maxX, x);
			cbox.maxY = std::max(cbox.maxY, y);
		}

		return cbox;
	}

	static uint16_t gen_u16_value(const float &x, const float &min, const float &max)
	{
		return static_cast<uint16_t>((x - min) / (max - min) * std::numeric_limits<uint16_t>::max());
	}
}
