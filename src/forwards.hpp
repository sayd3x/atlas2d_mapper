#pragma once

#include <memory>
#include <functional>
#include <string>

// Here are necessary forwards of some basic types

struct rect;
struct atlas_item;
struct atlas_props;
struct image_props;

class chain_node;
using chain_node_ptr = std::shared_ptr<chain_node>;

class atlas_builder;
using atlas_builder_ptr = std::shared_ptr<atlas_builder>;

class bin_packer;
using bin_packer_ptr = std::shared_ptr<bin_packer>;

