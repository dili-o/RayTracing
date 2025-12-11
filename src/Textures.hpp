#pragma once
#include "Vec3.hpp"
#include "Image.hpp"

class Texture {
public:
	virtual ~Texture() = default;
	virtual Color sample(real u, real v) const = 0;
};

class SolidTexture : public Texture {
public:
	SolidTexture(const Color& albedo) : albedo(albedo) {}

	Color sample(real u, real v) const override {
		return albedo;
	}

private:
	Color albedo;
};

class ImageTexture : public Texture {
public:
	ImageTexture(const std::string &filename) : image_data(filename.c_str()) {}

	Color sample(real u, real v) const override {
		i32 x = u * image_data.width();
		i32 y = v * image_data.height();

		const u8* pixel_data = image_data.pixel_data(x, y);
		const u8* pixel_data1 = pixel_data + 1;
		const u8* pixel_data2 = pixel_data + 2;

		if (!pixel_data || !pixel_data1 || !pixel_data2) {
			pixel_data = image_data.pixel_data(x, y);
		}

		real color_scale = 1.f / 255.f;
		return Color(pixel_data[0] * color_scale, pixel_data[1] * color_scale, pixel_data[2] * color_scale);
	}
private:
	Image image_data;
};