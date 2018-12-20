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

	struct Outline
	{
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
	};

	struct WIPCell
	{
		Rect bbox;
		uint32_t value;
		uint32_t from;
		uint32_t to;
		uint32_t startLength;
	};

	template<typename T>
	static inline void dyn_array_grow(T **data, uint32_t *capacity, size_t element_size)
	{
		*capacity = *capacity ? *capacity * 2 : 8;
		T *new_data = (T *) realloc(*data, *capacity * element_size);
		*data = new_data;
	}

	static void add_outline_point(Outline *o, const Vector2 &point);

	static void add_outline_contour(Outline *o, ContourRange *range);

	static void outline_add_odd_point(Outline *o);

	static Vector2 convert_point(const FT_Vector *v);

	static int move_to_func(const FT_Vector *to, Outline *o);

	static int line_to_func(const FT_Vector *to, Outline *o);

	static int conic_to_func(const FT_Vector *control, const FT_Vector *to, Outline *o);

	static int cubic_to_func(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, Outline *o);

	void outline_decompose(FT_Outline *outline, Outline *o);

	static uint32_t cell_add_range(uint32_t cell, uint32_t from, uint32_t to);

	static bool is_cell_filled(Outline *o, Rect *bbox);

	static bool wipcell_add_bezier(Outline *o, Outline *u, uint32_t i, uint32_t j, uint32_t contour_index, WIPCell *cell);

	static bool wipcell_finish_contour(Outline *o, Outline *u, uint32_t contour_index, WIPCell *cell, uint32_t *max_start_len);

	static bool for_each_wipcell_add_bezier(Outline *o, Outline *u, uint32_t i, uint32_t j, uint32_t contour_index, WIPCell *cells);

	static bool for_each_wipcell_finish_contour(Outline *o, Outline *u, uint32_t contour_index, WIPCell *cells, uint32_t *max_start_len);

	static void copy_wipcell_values(Outline *u, WIPCell *cells);

	static void init_wipcells(Outline *o, WIPCell *cells);

	static uint32_t outline_add_filled_line(Outline *o);

	static uint32_t make_cell_from_single_edge(uint32_t e);

	static void set_filled_cells(Outline *u, WIPCell *cells, uint32_t filled_cell);

	static bool try_to_fit_in_cell_count(Outline *o);

	static uint32_t uint32_to_pow2(uint32_t v);

	void outline_make_cells(Outline *o);

	void outline_subdivide(Outline *o);

	void outline_fix_thin_lines(Outline *o);

	void outline_convert(FT_Outline *outline, Outline *o, char c);

	void outline_destroy(Outline *o);

	void outline_cbox(Outline *o, Rect *cbox);

	static uint16_t gen_u16_value(float x, float min, float max);
}
