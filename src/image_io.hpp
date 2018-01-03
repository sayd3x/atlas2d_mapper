#pragma once

#include "forwards.hpp"

/// Reads image from file
bool read_image(std::string const& filename, image_props& props, bool load_pixels=false);

/// Writes image to file
bool write_image(std::string const& filename, image_props const& props);
