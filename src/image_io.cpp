#include "image_io.hpp"
#include "helpers.hpp"
#include <atlas2d/pixel_format.hpp>
#include <atlas2d/forwards.hpp>
#include <boost/filesystem.hpp>
#include <png.h>
#include <set>
#include <iostream>
#include <easylogging++.h>

#define MODULE_LOGGER "image_io"

using namespace ::std;
using namespace ::atlas2d;

namespace fs = boost::filesystem;

namespace {
    using img_writer = std::function<bool(std::string const&, image_props const&)>;
    using img_reader = std::function<bool(std::string const&, image_props&, bool)>;

    // default error handler
    void png_error(png_structp pngStruct, png_const_charp msg) {
        CLOG(ERROR, MODULE_LOGGER) << msg;
    }

    // default warning handler
    void png_warning(png_structp pngStruct, png_const_charp msg) {
        CLOG(WARNING, MODULE_LOGGER) << msg;
    }

    /**
     @brief Reads all necessary information of a specific png file.
     Use loadData=true if whole image's pixels is required, otherwise only
     generic information will be read.
     */
    bool readPng(std::string const& filename, image_props& props, bool loadData) {
        shared_ptr<FILE> fp(fopen(filename.c_str(), "rb"), [](FILE* fp){ fclose(fp); });
        if(!fp) {
            CLOG(ERROR, MODULE_LOGGER) << "Error openening " << filename << " for write";
            return false;
        }
        
        png_structp pngStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        png_infop pngInfo = png_create_info_struct(pngStruct);
        
        shared_ptr<png_struct> pngGuard(pngStruct, [&](png_structp){
            png_destroy_read_struct(&pngStruct, &pngInfo, NULL);
        });
        
        shared_ptr<png_bytep> rowPtrsGuard;
        atlas2d::raw_data_ptr pixelsGuard;
        
        if(!pngStruct || !pngInfo) {
            CLOG(ERROR, MODULE_LOGGER) << "Error initializing png struct";
            return false;
        }
        
        if(setjmp(png_jmpbuf(pngStruct))) {
            return false;
        }

        png_set_error_fn(pngStruct, nullptr, &png_error, &png_warning);
        
        png_init_io(pngStruct, fp.get());
        png_set_sig_bytes(pngStruct, 0);
        
        png_read_info(pngStruct, pngInfo);
        
        if(!loadData) {
            props.size.width = png_get_image_width(pngStruct, pngInfo);
            props.size.height = png_get_image_height(pngStruct, pngInfo);
            props.fmt = atlas2d::pixel_format::rgba8;

            return true;
        }

        png_uint_32 bitdepth   = png_get_bit_depth(pngStruct, pngInfo);
        png_uint_32 channels   = png_get_channels(pngStruct, pngInfo);
        png_uint_32 color_type = png_get_color_type(pngStruct, pngInfo);
        
        // Convert palette color to true color
        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(pngStruct);
        
        // Convert low bit colors to 8 bit colors
        if (png_get_bit_depth(pngStruct, pngInfo) < 8)
        {
            if (color_type==PNG_COLOR_TYPE_GRAY || color_type==PNG_COLOR_TYPE_GRAY_ALPHA)
                png_set_expand_gray_1_2_4_to_8(pngStruct);
            else
                png_set_packing(pngStruct);
        }
        
        if (png_get_valid(pngStruct, pngInfo, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(pngStruct);
        
        // Convert high bit colors to 8 bit colors
        if (bitdepth == 16)
            png_set_strip_16(pngStruct);
        
        // Convert gray color to true color
        if (color_type==PNG_COLOR_TYPE_GRAY || color_type==PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(pngStruct);
        
        png_set_interlace_handling(pngStruct);
        
        // Update the changes
        png_read_update_info(pngStruct, pngInfo);
        
        
        uint32_t width = (uint32_t)png_get_image_width(pngStruct, pngInfo);
        uint32_t height = (uint32_t)png_get_image_height(pngStruct, pngInfo);
        bitdepth = png_get_bit_depth(pngStruct, pngInfo);
        channels = png_get_channels(pngStruct, pngInfo);
        uint32_t bpp = channels * bitdepth / 8;
        uint32_t bytesInRow = width * bpp;
        bool hasAlpha = (channels == 4);

        pixelsGuard = atlas2d::raw_data_ptr((unsigned char*)malloc(sizeof(unsigned char) * width * height * bpp),
                                        [](unsigned char* p){free(p);});
        unsigned char* pixels = pixelsGuard.get();
        
        // Map each image's row to the pixels array
        {
            
            rowPtrsGuard = shared_ptr<png_bytep>((png_bytepp)malloc(sizeof(png_bytep)*height),
                                                     [](png_bytepp ptr){free(ptr);});
            png_bytepp rowPtrs = rowPtrsGuard.get();

            for (uint32_t i = 0; i < height; i++) {
                rowPtrs[i] = &pixels[i * bytesInRow];
            }
            
            png_read_image(pngStruct, rowPtrs);
        }

        props.pixels = pixelsGuard;
        props.size.width = width;
        props.size.height = height;
        props.fmt = hasAlpha ? atlas2d::pixel_format::rgba8 : atlas2d::pixel_format::rgb8;

        return true;
    }

    /// Writes the image to the filename png file.
    bool writePng(std::string const& filename, image_props const& image) {
        if(image.fmt != pixel_format::rgba8 &&
           image.fmt != pixel_format::rgb8) {
            return false;
        }
        
        shared_ptr<FILE> fp(fopen(filename.c_str(), "wb"), [](FILE* fp){ fclose(fp); });
        if(!fp) {
            CLOG(ERROR, MODULE_LOGGER) << "Error openening " << filename << " for write";
            return false;
        }
        
        png_structp pngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        png_infop pngInfo = png_create_info_struct(pngStruct);
        
        shared_ptr<png_struct> pngGuard(pngStruct, [&](png_structp){
            png_destroy_write_struct(&pngStruct, &pngInfo);
        });
        
        shared_ptr<png_bytep> rowPtrsGuard;
        atlas2d::raw_data_ptr pixelsGuard;
        
        if(!pngStruct || !pngInfo) {
            CLOG(ERROR, MODULE_LOGGER) << "Error initializing png struct";
            return false;
        }
        
        if(setjmp(png_jmpbuf(pngStruct))) {
            return false;
        }
        
        png_set_error_fn(pngStruct, nullptr, &png_error, &png_warning);
        
        png_init_io(pngStruct, fp.get());
        
        png_set_IHDR(pngStruct,
                     pngInfo,
                     image.size.width,
                     image.size.height,
                     8,
                     image.fmt == pixel_format::rgba8 ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE,
                     PNG_FILTER_TYPE_BASE);
        
        rowPtrsGuard = shared_ptr<png_bytep>(new png_bytep[image.size.height], [](png_bytepp ptr){
            delete [] ptr;
        });
        
        png_bytepp rowPtrs = rowPtrsGuard.get();
        unsigned char* pixels = image.pixels.get();
        uint32_t bpp = pixel_format_details(image.fmt).bpp;
        uint32_t bytesInRow = image.size.width * bpp;
        
        for (uint32_t i = 0; i < image.size.height; i++) {
            rowPtrs[i] = (png_bytep)(&pixels[i * bytesInRow]);
        }
        
        png_set_rows(pngStruct, pngInfo, rowPtrs);
        
        png_write_png(pngStruct, pngInfo, PNG_TRANSFORM_IDENTITY, NULL);
        
        return true;
    }
    
    /// Describes specific image format and acts as an item of a "set" container
    struct image_ext {
        using Self = image_ext;
        
        string ext;
        img_reader reader;
        img_writer writer;
        
        image_ext() { ;; }
        explicit image_ext(string _ext): ext(move(_ext)) { ;; }
        
        Self& set_ext(string arg) { ext = move(arg); return *this; }
        Self& set_reader(img_reader arg) { reader = move(arg); return *this; }
        Self& set_writer(img_writer arg) { writer = move(arg); return *this; }

        bool operator<(image_ext const& tbl) const {
            return ext < tbl.ext;
        }
    };
    
    /// Returns table of supported formats to load or save
    std::set<image_ext> const& ext_table() {
        static std::set<image_ext> table = {
            image_ext(".png").set_reader(&readPng).set_writer(&writePng)
        };
        
        return table;
    }
    
    
} // anonymous

/// Returns an image format entry by filename
image_ext const* find_image_ext(string const& filename) {
    auto const& table = ext_table();
    
    string ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(),
                   ext.begin(), ::tolower);
    
    auto pos = table.find(image_ext(ext));
    if(pos == table.end()) {
        return nullptr;
    }
    
    image_ext const& item = *pos;
    return &item;
}

bool read_image(std::string const& filename, image_props& props, bool load_pixels) {
    auto processor = find_image_ext(filename);
    if(!processor) {
        CLOG(ERROR, MODULE_LOGGER) << "Unknown format of the file " << filename;
        return false;
    }

    return processor->reader(filename, props, load_pixels);
}

bool write_image(std::string const& filename, image_props const& props) {
    auto processor = find_image_ext(filename);
    if(!processor) {
        CLOG(ERROR, MODULE_LOGGER) << "Unknown format of the file " << filename;
        return false;
    }

    return processor->writer(filename, props);
}
