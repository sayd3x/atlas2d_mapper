#pragma once

#include <atlas2d/forwards.hpp>
#include <atlas2d/pixel_format.hpp>
#include <atlas2d/image.hpp>
#include <string>

/// Simple rect
struct rect {
    int x=0, y=0, width=0, height=0;
    
    rect() { ;; }
    rect(int _x, int _y, int _w, int _h)
    : x(_x), y(_y), width(_w), height(_h)
    { ;; }
};

/// Generic image properties
struct image_props {
    atlas2d::size size;             ///< Image size
    atlas2d::pixel_format fmt;      ///< Pixel format
    atlas2d::raw_data_ptr pixels;   ///< Pixels array
};

/// Describes atlas item generic properties
struct atlas_item: image_props {
    std::string image_path;         ///< Relative path to item's image
    bool rotated=false;             ///< Is the image rotated
    rect box;                       ///< Rect to place the image into
};

/// Describes atlas properties
struct atlas_props {
    atlas2d::pixel_format fmt;      ///< Atlas pixel format
    bool premultipled = false;      ///< Has premultipled alpha
    int padding = 0;                ///< Padding between atlas items
    float occupancy = 0;            ///< Atlas ocuppancy factor (the value in the range [0,1])
    atlas2d::size size;             ///< Dimensions of the atlas
};




