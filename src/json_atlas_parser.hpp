#pragma once

#include "forwards.hpp"
#include "helpers.hpp"
#include <istream>

/// Atlas parser properties
struct json_parser_props {
    using props = json_parser_props;
    using image_reader = std::function<bool(atlas_item&)>;
    
    atlas_builder_ptr atlas_builder;    ///< Atlas builder
    image_reader read_image;            ///< Atlas item reader
    
    /// Sets atlas builder
    props& set_atlas_builder(atlas_builder_ptr arg) {atlas_builder=std::move(arg); return *this;}
    /// Sets image reader
    props& set_image_reader(image_reader arg) {read_image=std::move(arg); return *this;}
};

/**
 @brief Parses the whole atlas from json mapping.
 The atlas_stream provides json stream of atlas mapping.
 The strites_map provides json stream of sprites map.
 */
bool parse_json_atlas(std::istream& atlas_stream, std::istream& sprites_stream, json_parser_props const& props);
