#pragma once

#include "chain_node.hpp"

/// Atlas naming settings
struct atlas_naming_props {
    bool naming_after_dir = false;  ///< Names atlas after its parent directory name
};

/// The node is responsible for generating atlas file names.
class atlas_naming_node: public chain_node {
public:

    struct init_props: atlas_naming_props {
        using props = init_props;
        
        /// Names atlas after its parent directory name
        props& enable_naming_after_dir(bool arg=true) {naming_after_dir=arg; return *this;}
    };
    
    atlas_naming_node(atlas_naming_props const& props);
    virtual ~atlas_naming_node();
    
    bool begin_atlas(atlas_props const& extra_info) override;
    bool add_atlas_item(atlas_item const& item) override;
    bool end_atlas(bool finalize) override;
    void reset() override;
    
    /// Returns generated name for the current atlas
    std::string get_atlas_name() const;
    
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> _pimpl;
};
