#include "rbp_wrappers.hpp"
#include "helpers.hpp"

using namespace ::atlas2d;

namespace  {
    bool update_atlas_item(int base_width, int base_height, rbp::Rect const& rect, atlas_item& item) {
        bool isSameRect = \
        (rect.height == base_height && rect.width == base_width) ||
        (rect.height == base_width && rect.width == base_height);
        
        if(!isSameRect)
            return false;
        
        if(rect.width == 0 || rect.height == 0)
            return false;
        
        item.box.x = rect.x;
        item.box.y = rect.y;
        item.box.width = rect.width;
        item.box.height = rect.height;
        item.rotated = base_width == rect.height && base_width != rect.width;

        return true;
    }
}


bool max_rects_bin::packer::insert_square(int width, int height, atlas_item& item) {
    auto rect = rbp_packer().Insert(width, height, prefs().insert_heuristic);
    return update_atlas_item(width, height, rect, item);
}

void max_rects_bin::packer::clean_bin() {
    rbp_packer().Init(prefs().bin_width,
                      prefs().bin_height);
}


bool skyline_bin::packer::insert_square(int width, int height, atlas_item& item) {
    auto rect = rbp_packer().Insert(width, height, prefs().insert_heuristic);
    return update_atlas_item(width, height, rect, item);
}

void skyline_bin::packer::clean_bin() {
    rbp_packer().Init(prefs().bin_width,
                      prefs().bin_height,
                      prefs().use_waste_map);
}


bool guillotine_bin::packer::insert_square(int width, int height, atlas_item& item) {
    auto rect = rbp_packer().Insert(width, height,
                                    prefs().use_merge,
                                    prefs().insert_heuristic,
                                    prefs().split_heuristic);
    return update_atlas_item(width, height, rect, item);
}

void guillotine_bin::packer::clean_bin() {
    rbp_packer().Init(prefs().bin_width,
                      prefs().bin_height);
}
