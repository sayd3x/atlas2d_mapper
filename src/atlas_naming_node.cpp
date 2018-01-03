#include "atlas_naming_node.hpp"
#include "forwards.hpp"
#include "helpers.hpp"
#include <map>
#include <boost/filesystem.hpp>

using namespace ::atlas2d;
using namespace ::std;
namespace fs = boost::filesystem;

namespace {
    string defaultAtlasName = "atlas";
    
    string extract_atlas_name(atlas_item const& item) {
        auto name = fs::basename(fs::path(item.image_path).parent_path());
        return name.empty() || name == "/" || name == "\\" ? defaultAtlasName : name;
    }
    
}

struct atlas_naming_node::Pimpl: atlas_naming_props {
    atlas_props current_atlas;
    std::map<string, int> used_names;
    string last_name;
    bool next_atlas = false;
    
    void update_statistic(atlas_item const& item) {
        string name = naming_after_dir ? extract_atlas_name(item) : defaultAtlasName;
        if(name == last_name && !next_atlas)
            return;
        
        last_name = name;
        auto pos = used_names.find(name);
        if(pos == used_names.end()) {
            used_names.insert(make_pair(name, 0));
            return;
        }
        ++(pos->second);
    }

    string get_atlas_name() const {
        string filename = defaultAtlasName;
        int counter = 0;

        auto pos = used_names.find(last_name);
        if(pos != used_names.end()) {
            filename = pos->first;
            counter = pos->second;
        }

        if(counter)
            filename += to_string(counter);
        
        return filename;
    }
    
    void reset() {
        used_names.clear();
        last_name = string();
        next_atlas = false;
    }
    
    bool is_splitting_required(atlas_item const& item) const {
        if(!naming_after_dir || last_name.empty() || next_atlas)
            return false;
        
        string name = extract_atlas_name(item);
        return name != last_name;
    }
};

atlas_naming_node::atlas_naming_node(atlas_naming_props const& props): _pimpl(new Pimpl) {
    ((atlas_naming_props&)*_pimpl) = props;
}

atlas_naming_node::~atlas_naming_node() {
    ;;
}

bool atlas_naming_node::add_atlas_item(atlas_item const& item) {
    if(_pimpl->is_splitting_required(item)) {
        // In case of changing name of item's parent directory we need to create a new atlas
        safe_fwd().end_atlas(false);
        safe_fwd().begin_atlas(_pimpl->current_atlas);
    }
    
    // update the name usage information
    _pimpl->update_statistic(item);
    _pimpl->next_atlas = false;
    
    return safe_fwd().add_atlas_item(item);
}

string atlas_naming_node::get_atlas_name() const {
    return _pimpl->get_atlas_name();
}

bool atlas_naming_node::begin_atlas(atlas_props const& info) {
    // just save current atlas properties
    _pimpl->current_atlas = info;
    return safe_fwd().begin_atlas(info);
}


bool atlas_naming_node::end_atlas(bool finalize) {
    _pimpl->next_atlas = true;
    return safe_fwd().end_atlas(finalize);
}

void atlas_naming_node::reset() {
    _pimpl->reset();
    safe_fwd().reset();
}


