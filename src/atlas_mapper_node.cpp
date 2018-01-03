#include "atlas_mapper_node.hpp"
#include "helpers.hpp"
#include "bin_packer.hpp"
#include <list>
#include <tuple>
#include <cmath>
#include <cassert>
#include <easylogging++.h>

#define MODULE_LOGGER "atlas_mapper"

using namespace ::atlas2d;
using namespace ::std;

namespace {
    
    struct WeightedItem;
    using ItemSet = list<WeightedItem>;
    using ItemIndex = ItemSet::iterator;
    using IndexList = list<ItemIndex>;

    // Item with extra information about its dimensions
    struct WeightedItem: atlas_item {
        int square = 0;     ///< Square of the item
        int sqpow2Exp = 0;  ///< The best squared pow2 bin to place into
    };
    
    // Sorting items by suare
    bool sortBySquare(WeightedItem const& a, WeightedItem const& b) {
        return a.square > b.square;
    }
    
    // Sorting items by sqpow2Exp
    bool sortBySqpow2(WeightedItem const& a, WeightedItem const& b) {
        return make_tuple(a.sqpow2Exp, a.square) > make_tuple(b.sqpow2Exp, b.square);
    }

    // Atlas with extra information about its items and square
    struct AtlasTemplate: atlas_props {
        ItemSet items;          ///< The collection of weighted items
        int itemsSquare = 0;    ///< Calculated square of the atlas
    };

    // The bin class to place items into
    struct ActiveBin {
        int itemExtraPixels = 0;    ///< Extra paddings for each item
        bin_packer_ptr packer;      ///< Actual bin packer
        IndexList itemIndexes;      ///< Indexes of atlas items which where placed into the bin
        int itemsSquare = 0;        ///< Calculated square of the bin
        int minEdgeLen = 0;         ///< The minimum size of bin's edge
        
        // Rebuilds the bin
        bool rebuildBin() {
            packer->clean_bin();
            
            for(auto const& index : itemIndexes) {
                auto& item = *index;
                bool success = packer->insert_square(item.size.width + itemExtraPixels,
                                                  item.size.height + itemExtraPixels,
                                                  item);
                if(!success)
                    return false;
                
                // restore original size of the item
                item.box.width -= itemExtraPixels;
                item.box.height -= itemExtraPixels;

            }
            
            return true;
        }
        
        // Inserts an item taking into account the extra padding
        bool tryInsertItem(ItemIndex index) {
            auto& item = *index;
            
            bool success = packer->insert_square(item.size.width + itemExtraPixels,
                                              item.size.height + itemExtraPixels,
                                              item);
            if(success) {
                // increace cummulative square of the bin
                itemsSquare += item.square;
                // calculate the minimal edge of the bin
                minEdgeLen = (std::max)(minEdgeLen, (std::max)(item.box.width, item.box.height));

                // restore original item size
                item.box.width -= itemExtraPixels;
                item.box.height -= itemExtraPixels;
                
                itemIndexes.push_back(move(index));
                return success;
            }
            
            return success;
        }
        
        // Returns bin size
        size binSize() const {
            array<int,2> sz = {0, 0};
            if(packer)
                sz = packer->bin_dims();
            return size(sz[0], sz[1]);
        }
        
        // Returns bin occupancy
        float occupancy() const {
            return packer ? packer->occupancy() : 0;
        }
    };
    
    // Calculates squared pow2 of the smallest edge of the atlas
    int calcBestSqpow2ExpOf(size const& itemSize) {
        auto widthExp = log2((double)itemSize.width);
        auto heightExp = log2((double)itemSize.height);
        
        auto minExp = ceil((std::min)(widthExp, heightExp));
        auto maxExp = ceil((std::max)(widthExp, heightExp));

        return minExp >= (std::max)(widthExp, heightExp) ? (int)minExp : (int)maxExp;
    }
}

struct atlas_mapper_node::Pimpl: atlas_mapper_props {
    atlas_builder* mainChain = nullptr;
    AtlasTemplate atlasTmpl;    ///< Atlas template
    ActiveBin activeBin;        ///< Active bin for packing
    
    // Calculates item extra pixels (padding)
    int itemExtraPixels() const {
        return atlasTmpl.padding * 2;
    }
    
    // Returns the size of the item taking into account paddings between items
    size getItemExtraSize(atlas_item const& item) {
        auto extraLen = itemExtraPixels();
        return size(item.size.width + extraLen, item.size.height + extraLen);
    }
    
    // Calculates the best power of two size of a bin
    bool calcBestSqpow2Size(size& resultSize) {
        auto maxEdgeLen = (std::max)(atlasTmpl.size.width, atlasTmpl.size.height);
        if(maxEdgeLen <= 0 || atlasTmpl.itemsSquare <= 0)
            return false;
        
        // calculate minimal power of two value
        auto itemsSquareExp = log2((double)atlasTmpl.itemsSquare) / 2.0;
        auto atlasMaxExp = log2((double)maxEdgeLen);
        auto bestExp = (std::min)(itemsSquareExp, atlasMaxExp);

        auto fixedExp = ceil(bestExp);

        if(!atlasTmpl.items.empty()) {
            // take into account item's exp
            double itemMinExp = (double)atlasTmpl.items.front().sqpow2Exp;
            fixedExp = (std::max)(fixedExp, itemMinExp);
        }

        int bestSize = (int)pow(2.0, fixedExp);
        resultSize = size(bestSize, bestSize);

        return true;
    }
    

    // Tries to compact the active bin
    void compactBin(ActiveBin& bin) {
        // calculates bin's minimal, maximal and best edges
        const int minEdgeLen = bin.minEdgeLen;
        int maxEdgeLen = (std::max)(bin.binSize().width, bin.binSize().height);
        int bestEdgeLen = (int)floor(sqrt(bin.itemsSquare));
        bestEdgeLen = bestEdgeLen > minEdgeLen ? bestEdgeLen : minEdgeLen;
        
        // we will try to find the suitable edge by modifying the floatingEdge
        int floatingEdge = bestEdgeLen;
        
        bool lastRepackSuccess = true;
        while((maxEdgeLen - floatingEdge)/2 > 0) {
            // create the better-sized bin
            auto tempBin = ActiveBin();
            tempBin.itemExtraPixels = bin.itemExtraPixels;
            tempBin.packer = this->create_bin(floatingEdge, floatingEdge);
            
            // try to repack all items in the bin
            for(auto index : bin.itemIndexes) {
                lastRepackSuccess = tempBin.tryInsertItem(index);
                if(!lastRepackSuccess)
                    break;
            }
            
            if(!lastRepackSuccess) {
                // in case of fail try to modify the edge until it meets the requirements
                floatingEdge += (maxEdgeLen - floatingEdge) / 2;
                continue;
            }

            // replace the bin with the better one
            // and try to find the better edge on next step
            bin = move(tempBin);
            maxEdgeLen = floatingEdge;
            floatingEdge = bestEdgeLen;
        }
        
        // We need to update all items of the bin,
        // as a previous repack operation ruined its coordinates
        if(!lastRepackSuccess && !bin.rebuildBin()) {
            CLOG(ERROR, MODULE_LOGGER) << "Error compacting the bin";
        }
    }

    // Calculates active bin size
    size calcBinsSize() {
        size resultSize(atlasTmpl.size.width, atlasTmpl.size.height);
        
        switch (sizing_algo) {
            case atlas_sizing::squared_pow2_size:
                calcBestSqpow2Size(resultSize);
                break;
            default:
                break;
        }
        
        return resultSize;
    }
    
    // Builds an atlas
    void buildAtlas(bool finalize = false) {
        // Create the new active bin
        activeBin = ActiveBin();
        activeBin.itemExtraPixels = itemExtraPixels();
        size binSize = calcBinsSize();
        activeBin.packer = this->create_bin(binSize.width, binSize.height);
        
        // Fill the bin with items. Try to insert the biggest items first.
        // We have to insert all items from the biggest to the smallest one
        // in order to be sure the bin is filled optimally
        auto& atlasItems = atlasTmpl.items;
        for(auto it = atlasItems.begin(); it != atlasItems.end(); ++it) {
            activeBin.tryInsertItem(it);
        }
        
        if(sizing_algo == atlas_sizing::best_size) {
            // In case of best_size sizing algorithm we need to compact the bin as much as possible
            CLOG(INFO, MODULE_LOGGER) << "Original occupancy = " << activeBin.packer->occupancy();
            compactBin(activeBin);
        }
        
        // Store updated atlas occupancy
        atlasTmpl.occupancy = activeBin.packer->occupancy();
        CLOG(INFO, MODULE_LOGGER) << "Occupancy = " << atlasTmpl.occupancy;

        // Then we transfer content of the bin to an atlas
        atlas_props atlas = atlasTmpl;
        atlas.size = activeBin.binSize();
        mainChain->begin_atlas(atlas);
        for(auto const& itemIndex : activeBin.itemIndexes) {
            auto const& atlasItem = *itemIndex;
            mainChain->add_atlas_item(atlasItem);
            
            // Update atlas' data
            atlasTmpl.itemsSquare -= atlasItem.square;
            assert(atlasTmpl.itemsSquare >= 0);
            atlasItems.erase(itemIndex);
        }
        mainChain->end_atlas(atlasItems.empty() && finalize);
    }
    
    // Builds atlases for collected items
    bool buildAtlases(bool finalize = false) {
        // First of all we need to sort all items according to atlas sizing algorithm
        switch (sizing_algo) {
            case atlas_sizing::squared_pow2_size:
                atlasTmpl.items.sort(&sortBySqpow2);
                break;
            default:
                atlasTmpl.items.sort(&sortBySquare);
                break;
        }

        do {
            // Build each atlas until the item list is empty
            buildAtlas(finalize);
        } while(!atlasTmpl.items.empty());
        
        return true;
    }
    
    // Set the atlas' info
    void setAtlasTemplate(atlas_props const& atlas) {
        AtlasTemplate tmpl;
        (atlas_props&)tmpl = atlas;
        atlasTmpl = move(tmpl);
        
        // reset active bin
        activeBin = ActiveBin();
    }
    
    // Store the item
    void addAtlasItem(atlas_item const& item) {
        WeightedItem weighted;
        (atlas_item&)weighted = item;

        // calculate item's square weights
        size itemSize = getItemExtraSize(item);
        weighted.square = itemSize.height * itemSize.width;
        weighted.sqpow2Exp = calcBestSqpow2ExpOf(itemSize);

        // update atlas' square
        atlasTmpl.itemsSquare += weighted.square;
        
        atlasTmpl.items.push_back(move(weighted));
    }
};


atlas_mapper_node::atlas_mapper_node(atlas_mapper_props const& props): _pimpl(new Pimpl) {
    (atlas_mapper_props&)(*_pimpl) = props;
    
    auto& safeForwarder = this->safe_fwd();
    _pimpl->mainChain = &safeForwarder;
}

atlas_mapper_node::~atlas_mapper_node() {
    ;;
}

bool atlas_mapper_node::begin_atlas(atlas_props const& atlas) {
    _pimpl->setAtlasTemplate(atlas);
    return true;
}

bool atlas_mapper_node::add_atlas_item(atlas_item const& item) {
    auto const& atlas = _pimpl->atlasTmpl;
    int padding = _pimpl->itemExtraPixels();
    
    // Check item size
    if(atlas.size.width < item.size.width + padding ||
       atlas.size.height < item.size.height + padding) {
        CLOG(ERROR, MODULE_LOGGER) << "The sprite size is too big to fit";
        return false;
    }
    
    // Collect items
    _pimpl->addAtlasItem(item);
    return true;
}

bool atlas_mapper_node::end_atlas(bool finalize) {
    // Build atlases
    return _pimpl->buildAtlases(finalize);
}

void atlas_mapper_node::reset() {
    ;;
}
