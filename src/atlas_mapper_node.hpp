#pragma once

#include "chain_node.hpp"

/// Atlas mapper properties
struct atlas_mapper_props {
    using bin_factory = std::function<bin_packer_ptr(int,int)>;
    
    /// Atlas size algorithm
    enum atlas_sizing {
        constant_size = 0,  ///< Prefer strict size
        best_size,          ///< Choose the best size
        squared_pow2_size,  ///< Choose the best suqared power of two size
    };

    atlas_sizing sizing_algo = atlas_sizing::best_size; ///< Atlas size algorithm
    bin_factory create_bin;                             ///< Bin factory
    float sqpow2_factor = 0.0f;
};


/// The node maps sprites into a bunch of atlases
class atlas_mapper_node: public chain_node {
public:
    struct init_props: atlas_mapper_props {
        using props = init_props;
        
        /// Set atlas size algorithm
        props& set_algo(atlas_sizing arg) {sizing_algo = arg; return *this;}
        /// Set bin factory
        props& set_bin_factory(bin_factory arg) {create_bin=std::move(arg); return *this;}
    };
    
    explicit atlas_mapper_node(atlas_mapper_props const& props);
    virtual ~atlas_mapper_node();
    
    virtual bool begin_atlas(atlas_props const& atlas) override;
    virtual bool add_atlas_item(atlas_item const& item) override;
    virtual bool end_atlas(bool finalize) override;
    virtual void reset() override;
    
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> _pimpl;
};


