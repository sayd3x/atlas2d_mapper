#pragma once

#include "bin_packer.hpp"
#include <rbp/MaxRectsBinPack.h>
#include <rbp/SkylineBinPack.h>
#include <rbp/GuillotineBinPack.h>

namespace details {
    
    /// Implements a common functionality of the rbp packers
    template<typename PrefsT>
    class rbp_wrapper: public bin_packer {
    public:
        /// Returns packer's preferences
        PrefsT& prefs() { return _prefs; }

        /// Returns packer's preferences
        PrefsT const& prefs() const { return _prefs; }
        
        /// Returns the wrapped rbp packer
        typename PrefsT::rbp_packer& rbp_packer() { return _rbp; }

        /// Returns the wrapped rbp packer
        typename PrefsT::rbp_packer const& rbp_packer() const { return _rbp; }
        
        /// Returns bin's occupancy
        virtual float occupancy() const override { return _rbp.Occupancy(); }

        virtual std::array<int,2> bin_dims() const override {
            std::array<int,2> a = {prefs().bin_width, prefs().bin_height};
            return a;
        }

    protected:
        PrefsT _prefs;                      ///< Bin packer preferences
        typename PrefsT::rbp_packer _rbp;   ///< The wrapped packer
    };
    
    /// Basic preferences of a rbp bin
    template<typename Self>
    struct rbp_prefs {
        int bin_width=0, bin_height=0;
        
        Self& self() {return static_cast<Self&>(*this);}
        
        Self& set_bin_width(int arg) { bin_width = arg; return self(); }
        Self& set_bin_height(int arg) { bin_height = arg; return self(); }

    };
    
} // details

/// Wraps the MaxRectsBinPack class
struct max_rects_bin {
    
    /// Preferences of the MaxRectsBinPack algos
    struct prefs: details::rbp_prefs<prefs> {
        using rbp_packer = rbp::MaxRectsBinPack;
        using rbp_insert_heuristic = rbp::MaxRectsBinPack::FreeRectChoiceHeuristic;
        
        /// Insert heuristic of the MaxRectsBinPack packer
        rbp_insert_heuristic insert_heuristic = rbp_insert_heuristic::RectBottomLeftRule;
        
        /// Sets a specific insert heuristic
        prefs& set_insert_heuristic(rbp_insert_heuristic arg) { insert_heuristic = arg; return self(); }
    };
    
    /// Wrapper of the MaxRectsBinPack packer
    class packer: public details::rbp_wrapper<prefs> {
    public:
        bool insert_square(int width, int height, atlas_item& item) override;
        void clean_bin() override;
    };
};

/// Wraps the SkylineBinPack class
struct skyline_bin {
    
    /// Preferences of the SkylineBinPack algos
    struct prefs: details::rbp_prefs<prefs> {
        using rbp_packer = rbp::SkylineBinPack;
        using rbp_insert_heuristic = rbp::SkylineBinPack::LevelChoiceHeuristic;
        
        /// Insert heuristic of the SkylineBinPack packer
        rbp_insert_heuristic insert_heuristic = rbp_insert_heuristic::LevelMinWasteFit;
        
        /// The WasteMap feature
        bool use_waste_map = true;
        
        /// Sets a specific insert heuristic
        prefs& set_insert_heuristic(rbp_insert_heuristic arg) { insert_heuristic = arg; return self(); }
        
        /// Enables the waste map feature
        prefs& enable_waste_map(bool arg=true) { use_waste_map = arg; return self(); }
    };

    /// Wrapper of the SkylineBinPack packer
    class packer: public details::rbp_wrapper<prefs> {
    public:
        bool insert_square(int width, int height, atlas_item& item) override;
        void clean_bin() override;

    };
};

/// Wraps the GuillotineBinPack class
struct guillotine_bin {
    
    /// Preferences of the GuillotineBinPack algos
    struct prefs: details::rbp_prefs<prefs> {
        using rbp_packer = rbp::GuillotineBinPack;
        using rbp_insert_heuristic = rbp::GuillotineBinPack::FreeRectChoiceHeuristic;
        using rbp_split_heuristic = rbp::GuillotineBinPack::GuillotineSplitHeuristic;

        /// The Insert Heurictic
        rbp_insert_heuristic insert_heuristic = rbp_insert_heuristic::RectBestAreaFit;
        
        /// The Split Heuristic
        rbp_split_heuristic split_heuristic = rbp_split_heuristic::SplitMinimizeArea;
        
        /// The Merge feature
        bool use_merge = true;
        
        /// Sets a specific insert heuristic
        prefs& set_insert_heuristic(rbp_insert_heuristic arg) { insert_heuristic = arg; return self(); }
        
        /// Sets a specific split heuristic
        prefs& set_split_heuristic(rbp_split_heuristic arg) { split_heuristic = arg; return self(); }
        
        /// Enables the Merge feature
        prefs& enable_merge(bool arg=true) { use_merge = arg; return self(); }
    };
    
    /// Wrapper of the GuillotineBinPack packer
    class packer: public details::rbp_wrapper<prefs> {
    public:
        bool insert_square(int width, int height, atlas_item& item) override;
        void clean_bin() override;
        
    };
};


