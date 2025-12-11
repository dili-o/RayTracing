#pragma once
#include "Assert.hpp"
// Vendor
#include <Vendor/stb_image.h>

#define BYTES_PER_PIXEL 3

class Image {
public:
	Image(const char* filename) {
		i32 comp;
		stbi_set_flip_vertically_on_load(true);

		fdata = stbi_loadf(filename, &image_width, &image_height, &comp, BYTES_PER_PIXEL);
		HASSERT_MSGS(fdata, "Failed to load image: {}", filename);

		bytes_per_scanline = image_width * BYTES_PER_PIXEL;
		convert_to_bytes();
	}

	~Image() {
		delete bdata;
		free(fdata);
	}

	i32 width() const { return image_width; }
	i32 height() const { return image_height; }

	const u8* pixel_data(i32 x, i32 y) const {
		HASSERT(bdata);
		x = std::clamp(x, 0, image_width - 1);
		y = std::clamp(y, 0, image_height - 1);
		return bdata + ((x * BYTES_PER_PIXEL) + y * bytes_per_scanline);
	}

private:
	void convert_to_bytes() {
		// Convert the linear floating point pixel data to bytes, storing the resulting byte
		// data in the `bdata` member.

		int total_bytes = image_width * image_height * BYTES_PER_PIXEL;
		bdata = new u8[total_bytes];

		// Iterate through all pixel components, converting from [0.0, 1.0] float values to
		// unsigned [0, 255] byte values.
		for (i32 i = 0; i < total_bytes; ++i) {
			bdata[i] = float_to_byte(fdata[i]);
		}
	}

	static u8 float_to_byte(f32 value) {
		if (value <= 0.f)
			return 0;
		if (value >= 1.f)
			return 255;

		return static_cast<u8>(value * 256.f);
	}

private:
	i32 image_width = 0;
	i32 image_height = 0;
	f32 *fdata = nullptr;
	u8  *bdata = nullptr;
	i32 bytes_per_scanline = 0;
};
