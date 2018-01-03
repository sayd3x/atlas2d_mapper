#pragma once

#include "forwards.hpp"
#include <array>

/**
 @brief Generic interface of a bin packer.
 The packer optimaly fills a bin with a bunch of 2d items.
 */
class bin_packer {
public:
    virtual ~bin_packer() { ;; }
    
    /// Returns current occupancy of the bin. The value is in [0,1] range.
    virtual float occupancy() const = 0;
    
    /// Returns dimensions of the bin
    virtual std::array<int, 2> bin_dims() const = 0;
    
    /**
     * @brief Inserts the square [width, height] into the bin and updates the item with its position.
     * @return In case of success returns true and updates the item.
     */
    virtual bool insert_square(int width, int height, atlas_item& item) = 0;
    
    /// Erases all preverved squares of the bin
    virtual void clean_bin() = 0;

};

