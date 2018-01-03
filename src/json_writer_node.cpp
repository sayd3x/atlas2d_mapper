#include "json_writer_node.hpp"
#include "json_atlas_dict.hpp"
#include "helpers.hpp"
#include <atlas2d/pixel_format.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <easylogging++.h>

#include <sstream>
#include <fstream>

#define MODULE_LOGGER "json_writer"

using namespace ::std;
using namespace ::rapidjson;

namespace rj = ::rapidjson;

namespace {
    std::runtime_error atlas_write_error("Error writing JSON atlas");
    std::runtime_error sprites_map_write_error("Error writing sprites map");

    using Dict = json_atlas_dict;
    
    string extractSpriteName(string const& imageName) {
        string name = imageName.substr(0, imageName.find_last_of("."));
        auto basenamePos = name.find_last_of("/");
        if(basenamePos != string::npos) {
            name = name.substr(basenamePos+1);
        }
        
        return name;
    }
}

struct json_writer_node::Pimpl: json_writer_props {
    rj::Document spritesDoc;    ///< The document for sprites map
    rj::Document doc;           ///< The document for atlas map
    rj::Value itemsArray;       ///< Atlas items array
    
    void reset() {
        resetJsonContent();
        spritesDoc = Document(kObjectType);
    }
    
    void resetJsonContent() {
        doc = Document(kObjectType);
        itemsArray = Value(kArrayType);
    }
    
    // Writes content of the document to a stream provided by the gen_atlas_stream
    void onNextAtlas(bool writeJson=true) {
        if(writeJson) {
            // Write the document to the stream
            auto outs = this->gen_atlas_stream();
            if(!outs) {
                CLOG(ERROR, MODULE_LOGGER) << "Invalid JSON stream!";
                throw atlas_write_error;
            }
            OStreamWrapper rjStream(*outs);
            
            PrettyWriter<OStreamWrapper> writer(rjStream);
            if(!doc.Accept(writer)) {
                CLOG(ERROR, MODULE_LOGGER) << "Error writing JSON atlas";
                throw atlas_write_error;
            }
        }
        // Prepare the node for a next atlas
        resetJsonContent();
    }
    
    // Registers the image name in the sprites map. Also extracts sprite name to the spriteName variable.
    bool addToSpriteMap(std::string const& imageFile, string& spriteName) {
        spriteName = extractSpriteName(imageFile);

        if(spritesDoc.FindMember(spriteName.c_str()) != spritesDoc.MemberEnd())
            return false;
        
        auto& allocator = spritesDoc.GetAllocator();
        spritesDoc.AddMember(rj::Value(spriteName.c_str(), allocator).Move(),
                             rj::Value(imageFile.c_str(), allocator).Move(),
                             allocator);
        
        return true;
    }
    
    // Writes sprites map to a stream provided by the gen_spritesmap_stream
    void writeSpritesMap() {
        if(!spritesDoc.MemberCount())
            return;
        
        auto outs = this->gen_spritesmap_stream();
        if(!outs) {
            CLOG(ERROR, MODULE_LOGGER) << "Invalid sprites map stream!";
            throw sprites_map_write_error;
        }
        
        OStreamWrapper rjStream(*outs);
        PrettyWriter<OStreamWrapper> writer(rjStream);
        if(!spritesDoc.Accept(writer)) {
            CLOG(ERROR, MODULE_LOGGER) << "Error writing sprites map";
            throw sprites_map_write_error;
        }
    }
};

json_writer_node::json_writer_node(json_writer_props const& props)
: _pimpl(new Pimpl)
{
    ((json_writer_props&)*_pimpl) = props;
    _pimpl->reset();
}

json_writer_node::~json_writer_node() {
    ;;
}

bool json_writer_node::add_atlas_item(atlas_item const& item) {
    auto& allocator = _pimpl->doc.GetAllocator();

    // Register item in the sprites map
    string spriteName;
    if(!_pimpl->addToSpriteMap(item.image_path, spriteName)) {
        // Don't process dublicated items
        CLOG(ERROR, MODULE_LOGGER)
            << "Sprite name "
            << spriteName
            << " already exists";
        return false;
    }
    
    // Fill the items array with item's properties
    rj::Value rectEntry(rj::kArrayType);
    rectEntry.PushBack(rj::Value(item.box.x), allocator);
    rectEntry.PushBack(rj::Value(item.box.y), allocator);
    rectEntry.PushBack(rj::Value(item.box.width), allocator);
    rectEntry.PushBack(rj::Value(item.box.height), allocator);

    rj::Value itemEntry(rj::kObjectType);
    itemEntry.AddMember(StringRef(Dict::region_rect), rectEntry, allocator);
    itemEntry.AddMember(StringRef(Dict::region_rotated), rj::Value(item.rotated).Move(), allocator);
    itemEntry.AddMember(StringRef(Dict::region_sprite_name), rj::Value(spriteName.c_str(), allocator).Move(),
                        allocator);

    _pimpl->itemsArray.PushBack(itemEntry, allocator);
    
    return safe_fwd().add_atlas_item(item);
}

bool json_writer_node::begin_atlas(atlas_props const& atlas) {
    auto& allocator = _pimpl->doc.GetAllocator();
    auto& body = _pimpl->doc;
    
    // Fill the document with atlas properties
    body.AddMember(StringRef(Dict::padding), rj::Value(atlas.padding).Move(), allocator);
    
    rj::Value size(rj::kArrayType);
    size.PushBack(rj::Value(atlas.size.width).Move(), allocator);
    size.PushBack(rj::Value(atlas.size.height).Move(), allocator);
    body.AddMember(StringRef(Dict::size), size, allocator);
    body.AddMember(StringRef(Dict::premiltipled), rj::Value(atlas.premultipled).Move(), allocator);
    body.AddMember(StringRef(Dict::pixel_format), StringRef(atlas2d::pixel_format_details(atlas.fmt).formatName.c_str()), allocator);
    
    auto spritesMapFilename = _pimpl->sprites_map_filename;
    if(spritesMapFilename.empty())
        return false;
    
    body.AddMember(StringRef(Dict::sprites_file), Value(spritesMapFilename.c_str(), allocator).Move(), allocator);

    return safe_fwd().begin_atlas(atlas);
}

bool json_writer_node::end_atlas(bool finalize) {
    auto& allocator = _pimpl->doc.GetAllocator();
    auto& body = _pimpl->doc;

    // Add the items array to the document
    body.AddMember(StringRef(Dict::regions), _pimpl->itemsArray, allocator);
    
    // Write atlas' content to a stream
    auto isAtlasEmpty = _pimpl->itemsArray.Empty();
    _pimpl->onNextAtlas(!isAtlasEmpty);
    
    if(finalize) {
        // Write sprites map on final stage
        _pimpl->writeSpritesMap();
    }
    
    return safe_fwd().end_atlas(finalize);
}

void json_writer_node::reset() {
    _pimpl->reset();
    safe_fwd().reset();
}

