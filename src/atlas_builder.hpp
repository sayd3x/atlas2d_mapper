#pragma once

#include "forwards.hpp"

/// Generic interface for building atlases.
class atlas_builder {
public:
    virtual ~atlas_builder() { ;; }
    
    /// Creates a new atlas and set it as active
    virtual bool begin_atlas(atlas_props const& props) = 0;
    
    /// Inserts an item to the active atlas
    virtual bool add_atlas_item(atlas_item const& item) = 0;
    
    /// Finishes building of the active atlas.
    virtual bool end_atlas(bool finalize) = 0;
    
    /// Resets builder settings
    virtual void reset() { ;; }
};
