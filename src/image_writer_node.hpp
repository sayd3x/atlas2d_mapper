#pragma once

#include "chain_node.hpp"

/// Image writer properties
struct image_writer_props {
    using img_writer = std::function<bool(image_props const&)>;
    
    img_writer writer;  ///< Handler to write the final image
};

/// The node to build the image of an atlas
class image_writer_node: public chain_node {
public:
    struct init_props: image_writer_props {
        using props = init_props;
        
        /// Handler to write the final image
        props& set_writer(img_writer arg) {writer = std::move(arg); return *this;}
    };
    
    explicit image_writer_node(image_writer_props const& props);
    virtual ~image_writer_node();
    
    bool begin_atlas(atlas_props const& atlas) override;
    bool add_atlas_item(atlas_item const& item) override;
    bool end_atlas(bool finalize) override;
    void reset() override;
    
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> _pimpl;
};
