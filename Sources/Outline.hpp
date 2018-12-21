#pragma once

#include <vector>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbbox.h>
#include <freetype/ftoutln.h>
#include "Maths/Vector2.hpp"
#include "Geometry.hpp"

namespace acid
{
#define FT_CHECK(r) do { FT_Error err = (r); assert(!err); } while (0)

#define OUTLINE_MAX_POINTS (255 * 2)

	struct ContourRange
	{
		uint32_t begin, end;
	};

	class Cell;

	class Outline
	{
	public:
		Rect bbox;

		Vector2 *points;
		uint32_t pointCount;
		uint32_t pointCapacity;

		ContourRange *contours;
		uint32_t contourCount;
		uint32_t contourCapacity;

		uint32_t *cells;
		uint32_t cellCountX;
		uint32_t cellCountY;

		void add_outline_point(const Vector2 &point);

		void add_outline_contour(ContourRange *range);

		void outline_add_odd_point();

		void outline_decompose(FT_Outline *outline);

		bool is_cell_filled(const Rect &bbox) const;

		void copy_wipcell_values(Cell *wipCells);

		void init_wipcells(Cell *wipCells);



		uint32_t outline_add_filled_line();

		bool try_to_fit_in_cell_count();

		void outline_make_cells();

		void outline_subdivide();

		void outline_fix_thin_lines();

		void outline_convert(FT_Outline *outline);

		void outline_destroy();

		Rect outline_cbox() const;
	};

	template<typename T>
	static inline void dyn_array_grow(T **data, uint32_t *capacity, size_t element_size)
	{
		*capacity = *capacity ? *capacity * 2 : 8;
		T *new_data = (T *) realloc(*data, *capacity * element_size);
		*data = new_data;
	}

	static Vector2 convert_point(const FT_Vector *v);


	static int move_to_func(const FT_Vector *to, Outline &o);

	static int line_to_func(const FT_Vector *to, Outline &o);

	static int conic_to_func(const FT_Vector *control, const FT_Vector *to, Outline &o);

	static int cubic_to_func(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, Outline &o);




	static uint32_t make_cell_from_single_edge(const uint32_t &e);

	static uint32_t uint32_to_pow2(const uint32_t &v);

	static uint16_t gen_u16_value(const float &x, const float &min, const float &max);
}
