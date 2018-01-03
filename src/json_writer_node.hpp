#pragma once

#include "chain_node.hpp"

/// Json writer properties
struct json_writer_props {
    using ostream_ptr = std::shared_ptr<std::ostream>;
    using ostream_generator = std::function<ostream_ptr()>;

    ostream_generator gen_atlas_stream;         ///< Stream factory for storing atlas content
    ostream_generator gen_spritesmap_stream;    ///< Stream factory for storing atlas sprites map
    std::string sprites_map_filename;           ///< Atlas sprites map filename
};

/// The node dumps atlas mapping to Json format
class json_writer_node: public chain_node {
public:
    struct init_props: json_writer_props {
        using props = init_props;
        
        /// Sets stream factory for storing atlas content
        props& set_atlas_stream_generator(ostream_generator arg) {gen_atlas_stream=std::move(arg); return *this;}
        /// Sets stream factory for storing atlas sprites map
        props& set_spritesmap_generator(ostream_generator arg) {gen_spritesmap_stream=std::move(arg); return *this;}
        /// Sets atlas sprites map filename
        props& set_spritesmap_filename(std::string arg) {sprites_map_filename=std::move(arg); return *this;}
    };
    
    explicit json_writer_node(json_writer_props const& props);
    virtual ~json_writer_node();

    bool begin_atlas(atlas_props const& atlas) override;
    bool add_atlas_item(atlas_item const& item) override;
    bool end_atlas(bool finalise) override;
    void reset() override;
    
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> _pimpl;
};
