
#ifndef RAPTOR_GLSL_SCENE_H
#define RAPTOR_GLSL_SCENE_H

// Scene common code

layout ( std140, set = MATERIAL_SET, binding = 0 ) uniform SceneConstants {
    mat4        inverse_view;
    mat4        inverse_projection;
    mat4  		inverse_view_projection;
    mat4        view_matrix;
    mat4        projection_matrix;
    mat4        previous_view_projection;

    vec4        eye;
    vec4        eye_debug;
    vec4        light;

    uint        sphere_count;
    uint        material_count;
    uint        dither_texture_index;
    float       z_near;

    float       z_far;
    float       projection_00;
    float       projection_11;
    uint        frustum_cull_meshes; // Bools

    uint        frustum_cull_meshlets; // Bools
    uint        occlusion_cull_meshes; // Bools
    uint        occlusion_cull_meshlets; // Bools
    uint        freeze_occlusion_camera;

    vec2        resolution;
    float       aspect_ratio;
    float       pad0001;

    vec4        frustum_planes[6];
};

#endif // RAPTOR_GLSL_SCENE_H
