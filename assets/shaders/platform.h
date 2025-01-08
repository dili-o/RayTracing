// Header file for common defines

// Global glsl version ///////////////////////////////////////////////////
#version 450

#define GLOBAL_SET 0
#define MATERIAL_SET 1

#define BINDLESS_BINDING 10
#define BINDLESS_IMAGES 11

#extension GL_ARB_shader_draw_parameters : enable

// Bindless support //////////////////////////////////////////////////////
// Enable non uniform qualifier extension
#extension GL_EXT_nonuniform_qualifier : enable
// Global bindless support. This should go in a common file.

layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform sampler2D global_textures[];
// Alias textures to use the same binding point, as bindless texture is shared
// between all kind of textures: 1d, 2d, 3d.
layout ( set = GLOBAL_SET, binding = BINDLESS_BINDING ) uniform sampler3D global_textures_3d[];

// Writeonly images do not need format in layout
layout( set = GLOBAL_SET, binding = BINDLESS_IMAGES ) writeonly uniform image2D global_images_2d[];


// Common constants //////////////////////////////////////////////////////
const float pi = 3.14159265359f;
const float two_pi = 2.0f * pi;
#define INVALID_TEXTURE_INDEX 65535

