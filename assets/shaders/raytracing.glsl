#if defined(COMPUTE)

layout(set = MATERIAL_SET, binding = 1) uniform writeonly image2D out_image;

layout( push_constant ) uniform constants
{
	vec2 src_image_size;
};


layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 screen_size = ivec2((gl_NumWorkGroups.xy * 8) - 1);//imageSize(out_image);

    float viewport_height = 2.0;
    float viewport_width = viewport_height * (float(screen_size.x)/screen_size.y);
    float focal_length = 0.001f; // Camera z distance to the viewport
    
    vec3 camera_center = eye.xyz;//vec3(0, 6, 0);

    // Calculate the vectors across the horizontal and down the vertical viewport edges.
    vec3 viewport_u = vec3(viewport_width, 0, 0);
    vec3 viewport_v = vec3(0, -viewport_height, 0);

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    vec3 pixel_delta_u = viewport_u / screen_size.x;
    vec3 pixel_delta_v = viewport_v / screen_size.y;

    // Calculate the location of the upper left pixel.
    vec3 viewport_upper_left = camera_center
                             - vec3(0, 0, focal_length) - viewport_u/2 - viewport_v/2;
    vec3 pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    vec3 pixel_center = pixel00_loc + (pos.x * pixel_delta_u) + (pos.y * pixel_delta_v);

    Ray ray;
    ray.origin = camera_center;//vec3(0.0f, 0.0f, 0.0f);

    vec4 target = inverse_projection * vec4((pixel_center - camera_center).xy, 1, 1);


    ray.direction = (inverse_view * vec4(normalize(vec3(target.xyz / target.w)).xyz, 0)).xyz;
    //ray.direction = pixel_center - camera_center;

    float a = 0.5 * normalize(ray.direction).y + 1.0;
    vec3 color = vec3(1.0) * (1.0 - a) + a * vec3(0.5, 0.7, 1.0);

    HitRecord rec;
    if (hit_world(ray, 0, 1000.f, rec)){
        color = 0.5f * (rec.normal + vec3(1.0f));
    }

    imageStore(out_image, pos, vec4(color.xyz, 1.0));
}


#endif // COMPUTE