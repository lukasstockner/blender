#ifndef __BPX_PACKED_LAYOUT_H__
#define __BPX_PACKED_LAYOUT_H__

#include <algorithm>
#include <cassert>
#include <vector>

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
		Item(const int u_res, const int v_res)
			: u_res(u_res), v_res(v_res), id(-1), x(-1), y(-1)
		{}

		int u_res;
		int v_res;

		int id;
		int x;
		int y;
	};

	typedef std::vector<Item> Items;

	BPXPackedLayout(const int count)
		: width(0), height(0), u_max_res(0), v_max_res(0)
	{
		items.reserve(count);
	}

	void add_item(const Item &item)
	{
		items.push_back(item);
		items.back().id = items.size() - 1;
		u_max_res = std::max(u_max_res, item.u_res);
		v_max_res = std::max(v_max_res, item.v_res);
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
		width = std::max(u_max_res + 2, 1024);

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

			// Check if enough room on this row
			if (dst_x + item.u_res + 2 * border > width) {
				// Move to next row
				assert(yinc != 0);
				dst_y += yinc;
				yinc = 0;
				dst_x = 0;
			}

			// Write final position
			item.x = dst_x + border;
			item.y = dst_y + border;

			dst_x += item.u_res + (2 * border);
			height = std::max(height, dst_y + item.v_res + (2 * border));
			max_width = std::max(dst_x, max_width);

			yinc = std::max(yinc, item.v_res + (2 * border));
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
		if (a.v_res == b.v_res) {
			return a.u_res > b.u_res;
		}
		else {
			return a.v_res > b.v_res;
		}
	}

	Items items;

	int width;
	int height;

	int u_max_res;
	int v_max_res;
};

#endif
