#if defined(COMPUTE)

const int k_num_bounces = 25;
const int k_samples_per_pixel = 2;

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 screen_size = ivec2((gl_NumWorkGroups.xy * 8) - 1);

    float viewport_height = 2.0;
    float viewport_width = viewport_height * (float(screen_size.x)/float(screen_size.y));
    float focal_length = z_near; // Camera z distance to the viewport
    
    vec3 camera_center = eye.xyz;

    // Calculate the vectors across the horizontal and down the vertical viewport edges.
    vec3 viewport_u = vec3(viewport_width, 0, 0);
    vec3 viewport_v = vec3(0, -viewport_height, 0);

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    vec3 pixel_delta_u = viewport_u / screen_size.x;
    vec3 pixel_delta_v = viewport_v / screen_size.y;

    // Calculate the location of the upper left pixel.
    vec3 viewport_upper_left = camera_center
                             - vec3(0, 0, focal_length) 
                             - viewport_u/2 
                             - viewport_v/2;
    vec3 pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    vec3 color = vec3(0.0f);

    uint seed = uint(uint(pos.x) * rng_state + uint(pos.y) * uint(9277) + frame_count * uint(26699)) | uint(1);
    Interval ray_interval = Interval(z_near, z_far, float[](0, 0));

    for (int i = 0; i < k_samples_per_pixel; ++i) {
        // Generate a random jitter within the pixel
        float jitter_x = rand(seed) - 0.5;
        float jitter_y = rand(seed) - 0.5;

        vec3 pixel_center = pixel00_loc 
                          + (pos.x + jitter_x) * pixel_delta_u 
                          + (pos.y + jitter_y) * pixel_delta_v;

        Ray ray;
        ray.origin = camera_center;

        // Pixel location in view space
        vec4 target = inverse_projection * vec4((pixel_center - camera_center).xy, 1, 1);
        ray.direction = (inverse_view * vec4(normalize(vec3(target.xyz)).xyz, 0)).xyz;
        ray.inv_dir = 1.0f / ray.direction;
        
        HitRecord rec;
        rec.t = z_far;
        vec3 hit_attenuation = vec3(1.0f);
        vec3 attenuation = vec3(1.0f);
        for(int j = 0; j < k_num_bounces; j++){
            Ray scattered_ray;
            vec3 current_attenuation = vec3(1.0f);
            sphere_test++;
#define BVH 0
#if BVH
            if(traverse_BVH(ray, z_near, z_far, rec, hit_attenuation)){
                switch(rec.mat.type){
                    case METAL:
                        scatter_metal(seed, rec.mat, ray, rec, current_attenuation, scattered_ray);
                        break;
                    case DIELECTRIC:
                        scatter_dielectric(seed, rec.mat, ray, rec, current_attenuation, scattered_ray);
                        break;
                    case LAMBERTIAN:
                        scatter_lambertian(seed, rec.mat, ray, rec, current_attenuation, scattered_ray);
                        break;
                }
                attenuation *= current_attenuation;
                ray = scattered_ray;
            }else{
                float a = 0.5 * normalize(ray.direction).y + 1.0;
                color += vec3(1.0) * (1.0 - a) + a * vec3(0.5, 0.7, 1.0);
                break;
            }
#else
            if (hit_world(ray, z_near, z_far, rec, 0, int(sphere_count))){
                switch(rec.mat.type){
                    case METAL:
                        scatter_metal(seed, rec.mat, ray, rec, current_attenuation, scattered_ray);
                        break;
                    case DIELECTRIC:
                        scatter_dielectric(seed, rec.mat, ray, rec, current_attenuation, scattered_ray);
                        break;
                    case LAMBERTIAN:
                        scatter_lambertian(seed, rec.mat, ray, rec, current_attenuation, scattered_ray);
                        break;
                }
                attenuation *= current_attenuation;
                ray = scattered_ray;
            }else{
                float a = 0.5 * normalize(ray.direction).y + 1.0;
                color += vec3(1.0) * (1.0 - a) + a * vec3(0.5, 0.7, 1.0);
                break;
            }
#endif
        }
        color *= attenuation * hit_attenuation;
        //color = vec3(float(sphere_test) / 4000.f, 0.f, 0.f);
    }

    vec3 current_color = imageLoad(accumulated_image, pos).xyz;

    vec3 new_color = current_color + color;
    imageStore(accumulated_image, pos, vec4(new_color.xyz, 1.f));

    new_color /= float(frame_count * k_samples_per_pixel);
    new_color = linear_to_gamma(new_color);
    imageStore(out_image, pos, vec4(new_color.xyz, 1.f));
}



#endif // COMPUTE