#include "json_atlas_parser.hpp"
#include "atlas_builder.hpp"
#include "helpers.hpp"
#include "json_atlas_dict.hpp"
#include <atlas2d/pixel_format.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <easylogging++.h>

#define MODULE_LOGGER "json_parser"

using namespace ::rapidjson;
using namespace ::std;

// undef colliding windows definings
#ifdef GetObject
#undef GetObject
#endif

namespace {
    using Dict = json_atlas_dict;

    bool parseSpritesMap(std::istream& stream, map<string, string>& dict) {
        if(!stream)
            return false;
        
        IStreamWrapper rjStream(stream);
        Document doc;
        doc.ParseStream(rjStream);

        for(auto const& m : doc.GetObject()) {
            dict[m.name.GetString()] = m.value.GetString();
        }
        
        return true;
    }
    
} // anonymous

bool parse_json_atlas(std::istream& atlasStream, std::istream& spritesStream, json_parser_props const& props) {
    IStreamWrapper rjStream(atlasStream);
    Document doc;
    doc.ParseStream(rjStream);
    
    if(!doc.IsObject()) {
        // Invalid JSON stream
        return false;
    }
    
    // Parse sprites map
    map<string, string> spritesMap;
    if(!parseSpritesMap(spritesStream, spritesMap))
        return false;

    // Parse atlas info
    atlas_props atlas;
    
    atlas.fmt = atlas2d::pixel_format_details(doc[Dict::pixel_format].GetString()).format;
    if(atlas.fmt == atlas2d::pixel_format::unknown) {
        // Unknown pixel format
        CLOG(ERROR, MODULE_LOGGER) << "Unknown pixel format";
        return false;
    }
    
    Value const& jPadding = doc[Dict::padding];
    if(!jPadding.IsInt())
        return false;
    atlas.padding = jPadding.GetInt();
    
    Value const& jSize = doc[Dict::size];
    if(!jSize.IsArray())
        return false;
    atlas.size = atlas2d::size(jSize[0].GetInt(), jSize[1].GetInt());

    Value const& jPremultipled = doc[Dict::premiltipled];
    if(!jPremultipled.IsBool())
        return false;
    atlas.premultipled = jPremultipled.GetBool();


    Value const& jRegions = doc[Dict::regions];
    if(!jRegions.IsArray())
        return false;

    auto& writer = *(props.atlas_builder);
    if(!writer.begin_atlas(atlas))
        return false;

    // Parse atlas regions
    bool hasError = false;
    for(auto& jRegion : jRegions.GetArray()) {
        // Parse each region
        atlas_item item;
        
        Value const& jRegionRect = jRegion[Dict::region_rect];
        hasError = !jRegionRect.IsArray();
        if(hasError)
            break;
        item.box = rect(
            jRegionRect[0].GetInt(),
            jRegionRect[1].GetInt(),
            jRegionRect[2].GetInt(),
            jRegionRect[3].GetInt()
        );
        
        Value const& jRegionSpriteName = jRegion[Dict::region_sprite_name];
        hasError = !jRegionSpriteName.IsString();
        if(hasError)
            break;
        string spriteName = jRegionSpriteName.GetString();
        auto spriteMapPos = spritesMap.find(spriteName);
        item.image_path = spriteMapPos != spritesMap.end() ? spriteMapPos->second : "";
        
        Value const& jRegionRotated = jRegion[Dict::region_rotated];
        hasError = !jRegionRotated.IsBool();
        if(hasError)
            break;
        item.rotated = jRegionRotated.GetBool();
        
        hasError = !props.read_image(item);
        if(hasError) {
            CLOG(ERROR, MODULE_LOGGER)\
            << "Error reading the image "\
            << item.image_path
            << " of the sprite "
            << spriteName;
            break;
        }
        
        hasError = !writer.add_atlas_item(item);
        if(hasError) {
            CLOG(ERROR, MODULE_LOGGER)\
            << "Error processing the image "\
            << item.image_path
            << " of the sprite "
            << spriteName;
            break;
        }
    }
    
    if(hasError) {
        writer.reset();
        return false;
    }
    
    return writer.end_atlas(true);
}


