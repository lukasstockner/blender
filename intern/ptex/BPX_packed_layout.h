#ifndef __BPX_PACKED_LAYOUT_H__
#define __BPX_PACKED_LAYOUT_H__

#include <algorithm>
#include <cassert>
#include <vector>

#include "BPX_rect.h"

// TODO(nicholasbishop): there's a lot of room for improvement
// here. None of this data is saved to a file, so we should be able to
// safely improve it to improve performance, decrease memory usage,
// and fix filtering issues (e.g. by adding mipmaps).
//
// TODO(nicholasbishop): one paper to look at is "A Space-efficient
// and Hardware-friendly Implementation of Ptex".
//
// TODO(nicholasbishop): not sure if it's worth aiming for
// power-of-two output texture?
struct BPXPackedLayout {
	struct Item {
		Item(const BPXRect &rect, const int id)
		: rect(rect), id(id)
		{}

		const int width() const {
			return rect.xend - rect.xbegin;
		}

		const int height() const {
			return rect.yend - rect.ybegin;
		}

		BPXRect rect;
		int id;
	};

	typedef std::vector<Item> Items;

	BPXPackedLayout(const int count)
		: width(0), height(0), u_max_res(0), v_max_res(0)
	{
		items.reserve(count);
	}

	void add_rect(const BPXRect &rect)
	{
		const int id = items.size();
		items.push_back(Item(rect, id));
		const Item &item = items.back();
		u_max_res = std::max(u_max_res, item.width());
		v_max_res = std::max(v_max_res, item.height());
	}

	void finalize()
	{
		if (u_max_res == 0 || v_max_res == 0) {
			return;
		}

		// Sort items descending by v-res
		std::sort(items.begin(), items.end(), sort_res);

		// Decide on output width, TODO(nicholasbishop): extremely
		// arbitrary for now
		width = std::max(u_max_res + 2 * border, 4096);

		// For now only packing mipmap level zero

		// Calc layout
		height = 0;
		int dst_x = 0;
		int dst_y = 0;
		int yinc = 0;
		int max_width = 0;
		for (Items::iterator iter = items.begin();
			 iter != items.end(); ++iter) {
			Item &item = *iter;

			const int u_res = item.width();
			const int v_res = item.height();

			// Check if enough room on this row
			if (dst_x + u_res + 2 * border > width) {
				// Move to next row
				assert(yinc != 0);
				dst_y += yinc;
				yinc = 0;
				dst_x = 0;
			}

			// Write final position
			item.rect.xbegin = dst_x + border;
			item.rect.ybegin = dst_y + border;
			item.rect.xend = item.rect.xbegin + u_res;
			item.rect.yend = item.rect.ybegin + v_res;

			dst_x += u_res + (2 * border);
			height = std::max(height, dst_y + v_res + (2 * border));
			max_width = std::max(dst_x, max_width);

			yinc = std::max(yinc, v_res + (2 * border));
		}

		// TODO?
		width = max_width;

		std::sort(items.begin(), items.end(), sort_id);
	}

	const Items &get_items() const
	{
		return items;
	}

	const int get_width() const
	{
		return width;
	}

	const int get_height() const
	{
		return height;
	}

	// Width of filter border (in texels) around each packed face
	static const int border = 1;

private:
	static bool sort_id(const Item &a, const Item &b)
	{
		return a.id < b.id;
	}

	// Order *descending* by v_res, then u_res
	static bool sort_res(const Item &a, const Item &b)
	{
		if (a.height() == b.height()) {
			return a.width() > b.width();
		}
		else {
			return a.height() > b.height();
		}
	}

	Items items;

	int width;
	int height;

	int u_max_res;
	int v_max_res;
};

#endif
