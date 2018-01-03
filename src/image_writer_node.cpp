#include "image_writer_node.hpp"
#include "helpers.hpp"
#include "image_io.hpp"
#include <atlas2d/pixel_format.hpp>
#include <atlas2d/raw_image.hpp>
#include <png.h>
#include <easylogging++.h>

#define MODULE_LOGGER "image_writer"

using namespace ::atlas2d;
using namespace ::std;

struct image_writer_node::Pimpl: image_writer_props {
    raw_image rawImage;             ///< Raw image to map atlas items into
    bool premultipleAlpha = false;  ///< Alpha premultiple flag
};

image_writer_node::image_writer_node(image_writer_props const& props): _pimpl(new Pimpl) {
    (image_writer_props&)(*_pimpl) = props;
    reset();
}

image_writer_node::~image_writer_node() {
    ;;
}

void image_writer_node::reset() {
    _pimpl->premultipleAlpha = false;
}


bool image_writer_node::begin_atlas(atlas_props const& atlas) {
    // Init the rawImage with the atlas properties
    auto& rawImage = _pimpl->rawImage;
    rawImage.init(raw_image::init_props()
                  .set_dims(atlas.size)
                  .set_pixel_format(atlas.fmt)
                  .set_sprites_padding(atlas.padding)
                  .wipe_allocated_data());
    
    // ... and preserve alpha premultiple flag
    _pimpl->premultipleAlpha = atlas.premultipled;
    return safe_fwd().begin_atlas(atlas);
}

bool image_writer_node::add_atlas_item(atlas_item const& item) {
    // Init the pixel_area with item's properties
    raw_pixel_area area;
    area.init(raw_pixel_area::init_props()
              .set_dims(item.size)
              .set_pixel_format(item.fmt)
              .set_raw_data(item.pixels));
    
    // set rotator according to item's rotation
    area.set_rotator(item.rotated ?
                     raw_pixel_area::rotate_270_degree :
                     raw_pixel_area::rotate_0_degree);
    
    // fill the atlas with the pixel area by its offset
    auto& rawImage = _pimpl->rawImage;
    bool isOk = rawImage.fill_image(area,
                                    raw_image::filling_props()
                                    .set_offset(offset(item.box.x, item.box.y))
                                    .enable_premultiple(_pimpl->premultipleAlpha));
    if(!isOk)
        return false;
    
    return safe_fwd().add_atlas_item(item);
}

bool image_writer_node::end_atlas(bool finalize) {
    auto& rawImage = _pimpl->rawImage;

    image_props image;
    image.fmt = rawImage.props().format;
    image.size = rawImage.props().dimensions;
    image.pixels = details::unowned_ptr(rawImage.get_raw_pixels());

    // Write the final atlas image
    if(!_pimpl->writer(image))
        return false;
    
    return safe_fwd().end_atlas(finalize);
}

