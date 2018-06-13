#include "forwards.hpp"
#include "helpers.hpp"
#include "image_io.hpp"
#include "json_writer_node.hpp"
#include "image_writer_node.hpp"
#include "rbp_wrappers.hpp"
#include "json_atlas_parser.hpp"
#include "atlas_naming_node.hpp"
#include "atlas_mapper_node.hpp"
#include <atlas2d/pixel_format.hpp>
#include <rbp/MaxRectsBinPack.h>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <iostream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <easylogging++.h>

using namespace std;
using namespace atlas2d;

namespace fs = boost::filesystem;
namespace po = boost::program_options;
namespace ba = boost::adaptors;

INITIALIZE_EASYLOGGINGPP

namespace {

    using atlas_naming_node_ptr = std::shared_ptr<atlas_naming_node>;

    // Creates a bin with specific dimensions
    bin_packer_ptr createBinPacker(int atlasWidth, int atlasHeight) {
        // Create packer
        auto binPacker = make_shared<max_rects_bin::packer>();
        binPacker->prefs()
        .set_bin_width(atlasWidth)
        .set_bin_height(atlasHeight);
        binPacker->clean_bin();
        
        return binPacker;
    }
    
    // Creates atlas names generator node
    atlas_naming_node_ptr createAtlasNamingNode(po::variables_map const& vars) {
        const bool dirNaming = vars["dir-naming"].as<bool>();
        return make_shared<atlas_naming_node>(atlas_naming_node::init_props()
                                              .enable_naming_after_dir(dirNaming));
    }
    
    bool extractPackingAlgo(po::variables_map const& vars, atlas_mapper_props::atlas_sizing& algo) {
        const string packingMode(vars["bin-type"].as<string>());

        static const map<string, atlas_mapper_props::atlas_sizing> packingAlgos = {
            {"bestfit", atlas_mapper_props::best_size},
            {"constant", atlas_mapper_props::constant_size},
            {"sqpow2", atlas_mapper_props::squared_pow2_size},
        };
        
        auto pos = packingAlgos.find(packingMode);
        if(pos == packingAlgos.end()) {
            return false;
        }
        
        algo = pos->second;
        
        return true;
    }
    
    // Creates default atlas mapper
    chain_node_ptr createAtlasMapper(po::variables_map const& vars) {
        const std::string defaultSpritesMapFilename = "sprites_map.json";
        std::string const outDir(vars["dst"].as<string>());
        
        auto packingAlgo = atlas_mapper_props::best_size;
        if(!extractPackingAlgo(vars, packingAlgo)) {
            LOG(ERROR) << "Invalid packing algo was selected!";
            return nullptr;
        }
        
        // Put the naming node at the begining of the chain
        // It splits atlases by directory
        chain_node_ptr chain = createAtlasNamingNode(vars);
        chain_node_ptr nextNode = chain;
        
        // Bin packer packs input images into the banch of atlases
        auto binPackerNode = make_shared<atlas_mapper_node>(atlas_mapper_node::init_props()
                                                          .set_algo(packingAlgo)
                                                          .set_bin_factory(&createBinPacker));
        nextNode = nextNode->set_child(binPackerNode);

        
        // Extra naming node following bin packer in order to avoid atlas naming issues
        atlas_naming_node_ptr namingNode = createAtlasNamingNode(vars);
        nextNode = nextNode->set_child(namingNode);

        
        // The next node is in charge of writing results to JSON files
        weak_ptr<atlas_naming_node> weakNameingNode = namingNode;
        json_writer_props::ostream_generator atlasStreamGen = [outDir,weakNameingNode]() {
            auto nameGen = weakNameingNode.lock();
            assert(nameGen);
            
            string atlasName = nameGen->get_atlas_name() + ".json";
            auto file = fs::path(outDir) / atlasName;
            return make_shared<ofstream>(file.generic_string(), ios_base::binary);
        };
        json_writer_props::ostream_generator spritesMapStreamGen = [outDir, defaultSpritesMapFilename]() {
            auto file = fs::path(outDir) / defaultSpritesMapFilename;
            return make_shared<ofstream>(file.generic_string(), ios_base::binary);
        };
        
        auto jsonWriter = make_shared<json_writer_node>(json_writer_node::init_props()
                                                         .set_spritesmap_filename(defaultSpritesMapFilename)
                                                         .set_spritesmap_generator(spritesMapStreamGen)
                                                         .set_atlas_stream_generator(atlasStreamGen));
        nextNode = nextNode->set_child(jsonWriter);
        
        if(vars["debug-mapping"].as<bool>()) {
            // In case of debug we attach extra drawing node to visualize
            // packed atlases
            image_writer_props::img_writer imgWriter = [weakNameingNode, outDir](image_props const& img) {
                auto nameGen = weakNameingNode.lock();
                assert(nameGen);
                
                string name = nameGen->get_atlas_name() + ".png";
                auto filename = fs::path(outDir) / name;
                return write_image(filename.generic_string(), img);
            };
            nextNode = nextNode->set_child(make_shared<image_writer_node>(image_writer_node::init_props()
                                                                          .set_writer(imgWriter)));
        }

        // Return builded chain
        return chain;
    }
    
    // Build the image of an atlas map file
    int performBuildAtlas(po::variables_map const& vars) {
        fs::path const atlasMapFile(vars["build-atlas"].as<string>());
        fs::path const srcDir(vars["src"].as<string>());
        string const dstFile(vars["dst"].as<string>());

        // Create atlas builder to draw a mapped atlas
        auto writeImageFn = [dstFile](image_props const& img) {
            return write_image(dstFile, img);
        };
        auto atlas_builder = make_shared<image_writer_node>(image_writer_node::init_props()
                                                            .set_writer(writeImageFn));
        
        // Create image reader
        auto readImageFn = [srcDir](atlas_item& item) {
            fs::path filename = fs::path(srcDir) / item.image_path;
            filename.make_preferred();
            return read_image(filename.string(), item, true);
        };
        

        // Open necessary streams
        ifstream atlasStream(atlasMapFile.c_str(), std::ios_base::in | std::ios_base::binary);
        
        // TODO: fix using of default sprites file name
        auto spritesMapFile = atlasMapFile.parent_path() / "sprites_map.json";
        ifstream spritesStream(spritesMapFile.c_str(), std::ios_base::in | std::ios_base::binary);

        // Parse mapped atlas
        bool res = parse_json_atlas(atlasStream,
                                    spritesStream,
                                    json_parser_props()
                                    .set_atlas_builder(atlas_builder)
                                    .set_image_reader(readImageFn));
        
        if(!res) {
            LOG(ERROR) << "An error during parsing mapped atlas " << atlasMapFile;
            return 1;
        }
        
        return 0;
    }

    // Setup logging
    void initLogging(po::variables_map const& vars) {
        el::Loggers::addFlag(el::LoggingFlag::CreateLoggerAutomatically);
        
        el::Configurations conf;
        conf.setToDefault();
        if(!vars["verbose"].as<bool>()) {
            // Disable detailed logging by default
            conf.set(el::Level::Info, el::ConfigurationType::Enabled, "false");
            conf.set(el::Level::Warning, el::ConfigurationType::Enabled, "false");
            conf.set(el::Level::Verbose, el::ConfigurationType::Enabled, "false");
            conf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false");
            conf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false");
        }
        el::Loggers::setDefaultConfigurations(conf, true);
    }

} // anonymous


int main(int argc, const char * argv[]) {
    po::options_description desc("Json atlas options");
    desc.add_options()
        ("help", "Help message")
        ("verbose,v", po::bool_switch()->default_value(false), "Verbose mode")
        ("width,w", po::value<int>(), "Atlas width")
        ("height,h", po::value<int>(), "Atlas height")
        ("padding,p", po::value<int>()->default_value(0), "Padding between sprites")
        ("pixel-format", po::value<string>()->default_value("rgba8"), "Set preffered pixel format")
        ("bin-type", po::value<string>()->default_value("bestfit"), "Atlas packing algorithm [constant, bestfit, sqpow2]")
        ("debug-mapping", po::bool_switch()->default_value(false), "Draw the image of each atlas during builing of jsons")
        ("build-atlas", po::value<string>(), "Json atlas to build")
        ("premultiple-alpha", po::bool_switch()->default_value(false), "Premultiple alpha channel")
        ("dir-naming", po::bool_switch()->default_value(false), "Name json files after their parent directories")
        ("src", po::value<string>()->required(), "Source directory")
        ("dst", po::value<string>()->required(), "Output directory")
        ("filter,f", po::value<string>()->default_value(".*\\.png$"), "Image file filter")
    ;
    
    po::positional_options_description pos;
    pos.add("src", 1).add("dst", 1);
    
    // Parse command line arguments
    po::variables_map vars;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(pos).run(), vars);
    try {
        po::notify(vars);
    } catch( po::error const& e) {
        cout << e.what() << endl;
        cout << desc << endl;
        return 1;
    }
    
    // Check for the help argument
    if(vars.count("help") > 0) {
        cout << desc;
        return 1;
    }

    initLogging(vars);

    if(vars.count("build-atlas")) {
        // Build an atlas by json map
        LOG(INFO) << "Perform atlas building";
        return performBuildAtlas(vars);
    }

    // Performing atlas mapping mode
    
    auto is_file = [](fs::directory_entry const& e){
        return fs::is_regular_file(e.status());
    };

    const regex matchPattern(vars["filter"].as<string>(), regex_constants::grep);
    auto matched_pattern = [&matchPattern](fs::directory_entry const& e) {
        return regex_match(e.path().string(), matchPattern);
    };
    
    // create and set up the Atlas Mapper
    auto atlasMapper = createAtlasMapper(vars);
    if(!atlasMapper) {
        LOG(ERROR) << "Error during creating the default writer";
        return 1;
    }
    
    const string srcDir(vars["src"].as<string>());
    const int atlasWidth = vars["width"].as<int>();
    const int atlasHeight = vars["height"].as<int>();
    const int padding = vars["padding"].as<int>();
    const bool debugMapping = vars["debug-mapping"].as<bool>();
    const bool premultipled = vars["premultiple-alpha"].as<bool>();
    const pixel_format pixelFormat = pixel_format_details(vars["pixel-format"].as<string>()).format;

    if(pixelFormat == pixel_format::unknown) {
        LOG(ERROR) << "Unknown pixel format";
        return 1;
    }
    
    atlas_props atlas;
    atlas.size = size(atlasWidth, atlasHeight);
    atlas.padding = padding;
    atlas.fmt = pixelFormat;
    atlas.premultipled = premultipled;
    
    // data processing...
    LOG(INFO) << "Perform creating JSON atlases";
    atlasMapper->reset();
    if(!atlasMapper->begin_atlas(atlas)) {
        LOG(ERROR) << "Can't prepare the atlas for writing.";
        return 1;
    }
    
    // Process each image
    for(auto const& dirEntry : fs::recursive_directory_iterator(srcDir)
        | ba::filtered(is_file) | ba::filtered(matched_pattern))
    {
        auto imageFile = fs::relative(dirEntry.path(), srcDir);
        LOG(INFO) << "Processing " << imageFile;
        
        atlas_item item;
        item.image_path = imageFile.generic_string();
        
        fs::path filename = srcDir / imageFile;
        if(!read_image(filename.generic_string(), item, debugMapping)) {
            LOG(ERROR) << "Error reading the " << filename << " file";
            return 1;
        }
        
        if(!atlasMapper->add_atlas_item(item)) {
            LOG(ERROR)  << "Error during adding the sprite "
            << filename
            << " to the atlas";
            return 1;
        }
    }
    
    // Finalize atlas
    if(!atlasMapper->end_atlas(true)) {
        LOG(ERROR) << "Error during finalizing";
        return 1;
    }
    
    return 0;
}

