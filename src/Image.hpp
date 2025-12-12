#pragma once
#include "Assert.hpp"
#include <memory>
#include <Vendor/stb_image.h>

#define BYTES_PER_PIXEL 4

class Image {
public:
    Image(const char* filename) {
        i32 comp;
        stbi_set_flip_vertically_on_load(true);
        f32* raw_fdata = stbi_loadf(filename, &image_width, &image_height, &comp, BYTES_PER_PIXEL);
        HASSERT_MSGS(raw_fdata, "Failed to load image: {}", filename);
        fdata.reset(raw_fdata);  // Take ownership
        bytes_per_scanline = image_width * BYTES_PER_PIXEL;
        convert_to_bytes();
    }

    Image(u8* data, i32 width, i32 height) 
        : image_width(width), image_height(height),
          bdata(data),
          bytes_per_scanline(width * BYTES_PER_PIXEL) {}

    // Destructor now automatic with unique_ptr
    ~Image() = default;

    // Delete copy, allow move (or implement copy if needed)
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) = default;
    Image& operator=(Image&&) = default;

    i32 width() const { return image_width; }
    i32 height() const { return image_height; }
    
    const u8* pixel_data(i32 x, i32 y) const {
        HASSERT(bdata);
        x = std::clamp(x, 0, image_width - 1);
        y = std::clamp(y, 0, image_height - 1);
        return bdata.get() + ((x * BYTES_PER_PIXEL) + y * bytes_per_scanline);
    }

private:
    void convert_to_bytes() {
        int total_bytes = image_width * image_height * BYTES_PER_PIXEL;
        bdata.reset(static_cast<u8*>(malloc(total_bytes)));
        
        for (i32 i = 0; i < total_bytes; ++i) {
            bdata.get()[i] = float_to_byte(fdata.get()[i]);
        }
    }

    static u8 float_to_byte(f32 value) {
        if (value <= 0.f) return 0;
        if (value >= 1.f) return 255;
        return static_cast<u8>(value * 255.f);
    }

private:
    i32 image_width = 0;
    i32 image_height = 0;
    
    struct StbiFree { void operator()(void* p) { free(p); } };
    std::unique_ptr<f32[], StbiFree> fdata;
    std::unique_ptr<u8[], StbiFree> bdata;
    
    i32 bytes_per_scanline = 0;
};