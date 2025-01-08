/*--------------------------Structs--------------------------*/
struct Ray{
    vec3 origin;
    vec3 direction;
    float time;
    vec3 inv_dir;
};


struct Interval{ // 16 bytes
    float min;
    float max;
    float padding[2];
};

//struct AABB{ // 48 bytes
//    Interval x;
//    Interval y;
//    Interval z;
//    float padding[12];
//};

struct AABB{
    vec4 min;
    vec4 max;
};

struct BVHNode{
    uint sphere_index;
    uint sphere_count;
    uint node_child_index;
    uint should_draw;

    AABB bounding_box; // 48

    vec4 debug_color;
};

struct Material{
    vec3 albedo;
    uint type;
    float fuzz; // Acts as the refraction_index for dielctrics
    float padding[3];
};

const uint LAMBERTIAN   = 1 << 0;
const uint METAL        = 1 << 1;
const uint DIELECTRIC   = 1 << 2;

struct Sphere{
    vec3 origin;
    float radius;

    AABB bounding_box;

    uint material_index;
    uint padding[3];
};

struct HitRecord {
    vec3 position;
    vec3 normal;
    Material mat;
    float t; // Why need t if you already have the position?
    bool front_face;
};

struct SceneInfo{
    uint total_bounce_count;
};

/*------------------Compute Shader uniforms------------------*/
layout(set = MATERIAL_SET, binding = 1, rgba32f) uniform image2D accumulated_image;

layout(set = MATERIAL_SET, binding = 2) uniform writeonly image2D out_image;

layout ( set = MATERIAL_SET, binding = 3 ) readonly buffer Materials {
    Material materials[];
};

layout ( set = MATERIAL_SET, binding = 4 ) readonly buffer Spheres {
    Sphere spheres[];
};

layout ( set = MATERIAL_SET, binding = 5 ) readonly buffer BVHNodes {
    BVHNode nodes[];
};

layout ( set = MATERIAL_SET, binding = 6 ) writeonly buffer Scene_Info {
    SceneInfo scene_info;
};

layout( push_constant ) uniform constants
{
	uint node_index;
    uint pad;
    uint rng_state;
    uint frame_count;
    uint bvh_count;
};
///////////////////////////////////////////////////////////////

/*-------------------------Functions-------------------------*/
//
// Interval
Interval Interval_expand(Interval interval, float delta){
    float expand_padding = delta / 2.f;
    return Interval(interval.min - expand_padding, interval.max + expand_padding, float[](0, 0));
}
///////////////////////////////////////////////////////////////
// AABB
AABB AABB_create(vec3 a, vec3 b){
    AABB aabb;

    aabb.min = vec4(min(a, b), 1.0f);
    aabb.max = vec4(max(a, b), 1.0f);

    return aabb;
}

bool AABB_hit(AABB aabb, Ray r, Interval ray_t){
    for(int axis = 0; axis < 3; axis++){

        float min_v = aabb.min[axis];
        float max_v = aabb.max[axis];
        
        float t0 = (min_v - r.origin[axis]) * r.inv_dir[axis];
        float t1 = (max_v - r.origin[axis]) * r.inv_dir[axis];

        float t_min = min(t0, t1);
        float t_max = max(t0, t1);

        ray_t.min = max(ray_t.min, t_min);
        ray_t.max = min(ray_t.max, t_max);

        if (ray_t.max <= ray_t.min)
            return false;
    }
    return true;
}
///////////////////////////////////////////////////////////////
//
// Linear to Gamma
float linear_to_gamma(float linear_component)
{
    if (linear_component > 0.0f)
        return sqrt(linear_component);

    return 0.f;
}

vec3 linear_to_gamma(vec3 color_vector)
{
    return vec3(linear_to_gamma(color_vector.x), linear_to_gamma(color_vector.y), linear_to_gamma(color_vector.z));
}

// Random Number Generators
uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

// Range 0 to 1
float rand(inout uint seed) {
    return float(wang_hash(seed)) / 4294967296.0;
}

float rand_in_range(inout uint seed, float min, float max) {
    return min + (max - min) * rand(seed);
}

vec3 rand_unit_vector(inout uint seed) {
    float z = rand_in_range(seed, -1, 1);
    float a = rand(seed) * two_pi;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return vec3(x, y, z);
}

vec3 rand_on_hemisphere(inout uint seed, vec3 normal){
    vec3 unit_vec = rand_unit_vector(seed);
    if(dot(unit_vec, normal) > 0.0){
        return unit_vec;
    }else{
        return -unit_vec;
    }
}

// Ray Interactions
float schlick_reflect(float cosine, float refraction_index){
    float r0 = (1 - refraction_index) / (1 + refraction_index);
    r0 = r0*r0;
    return r0 + (1-r0) * pow((1 - cosine),5);
}

bool near_zero(vec3 v) {
    // Return true if the vector is close to zero in all dimensions.
    float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}
//
//
void scatter_lambertian(inout uint seed, Material mat, const Ray r_in, const HitRecord rec, inout vec3 attenuation, inout Ray scattered){
    vec3 scatter_direction = rec.normal + rand_unit_vector(seed);
    if(near_zero(scatter_direction))
        scatter_direction = rec.normal;

    Ray new_ray = Ray(rec.position, scatter_direction, 0.0f, 1.f / scatter_direction);
    scattered = new_ray;
    attenuation = mat.albedo;
}

void scatter_metal(inout uint seed, Material mat, const Ray r_in, const HitRecord rec, out vec3 attenuation, out Ray scattered){
    vec3 reflected_direction = reflect(r_in.direction, rec.normal);
    reflected_direction = normalize(reflected_direction) + (mat.fuzz * rand_unit_vector(seed));
    scattered = Ray(rec.position, reflected_direction, 0.0f, 1.f / reflected_direction);
    attenuation = mat.albedo;
}

void scatter_dielectric(inout uint seed, Material mat, const Ray r_in, const HitRecord rec, out vec3 attenuation, out Ray scattered){
    attenuation = vec3(1.0f);
    float ri = rec.front_face ? (1.0/mat.fuzz) : mat.fuzz;
    vec3 unit_direction = normalize(r_in.direction);

    float cos_theta = min(dot(-unit_direction, rec.normal), 1.0);
    float sin_theta = sqrt(1.0 - cos_theta*cos_theta);

    bool cannot_refract = ri * sin_theta > 1.0;
    vec3 direction;

    if (cannot_refract || schlick_reflect(cos_theta, ri) > rand(seed))
        direction = reflect(unit_direction, rec.normal);
    else
        direction = refract(unit_direction, rec.normal, ri);

    scattered = Ray(rec.position, direction, 0.0f, 1.f / direction);
}
//
//
bool hit_sphere(Ray ray, float ray_tmin, float ray_tmax, inout HitRecord hit_record, Sphere sphere){
    vec3 oc = sphere.origin - ray.origin;
    float a = length(ray.direction) * length(ray.direction);
    float h = dot(ray.direction, oc);
    float c = length(oc) * length(oc) - sphere.radius * sphere.radius;

    float discriminant = h*h - a*c;
    if (discriminant < 0.f){
        return false;
    }
    float sqrt_d = sqrt(discriminant);

    // Find the nearest t that lies in the acceptable range.
    float t = (h - sqrt_d) / a;
    if (t <= ray_tmin || t >= ray_tmax) {
        t = (h + sqrt_d) / a;
        if (t <= ray_tmin || t >= ray_tmax)
            return false;
    }

    hit_record.position = ray.origin + ray.direction * t;
    vec3 outward_normal = (hit_record.position - sphere.origin) / sphere.radius;

    hit_record.front_face = (dot(ray.direction, outward_normal) < 0.0f) ? true : false;
    hit_record.normal = hit_record.front_face ? outward_normal : -outward_normal;

    hit_record.t = t;
    hit_record.mat = materials[sphere.material_index];

    return true;
}
uint sphere_test = 0;
//
//
bool hit_world(Ray ray, float ray_tmin, float ray_tmax, inout HitRecord hit_record, int start, int end){
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_t_so_far = ray_tmax;

    for(int i = start; i < end; i++){
        sphere_test++;
        if(hit_sphere(ray, ray_tmin, ray_tmax, temp_rec, spheres[i])){
            hit_anything = true;
            if(temp_rec.t < closest_t_so_far){
                closest_t_so_far = temp_rec.t;
                hit_record = temp_rec;
            }
            
        }
    }
    return hit_anything;
}
//
//
bool traverse_BVH(Ray ray, float ray_tmin, float ray_tmax, inout HitRecord rec, inout vec3 attenuation){
    BVHNode node_stack[15];
    int stack_index = 0;
    node_stack[stack_index++] = nodes[0];
    
    float current_t = ray_tmax;

    bool hit = false;
    while(stack_index > 0){
        sphere_test++;
        BVHNode node = node_stack[--stack_index];
        atomicAdd(scene_info.total_bounce_count, 1);
        if(AABB_hit(node.bounding_box, ray, Interval(ray_tmin, ray_tmax, float[](0, 0)))){
            //if(node.should_draw == 1)
            //    attenuation = node.debug_color.xyz;

            if(node.node_child_index == 0){
                if(hit_world(ray, ray_tmin, current_t, rec, int(node.sphere_index), int(node.sphere_index + node.sphere_count))){
                    hit = true;
                    current_t = rec.t;
                }
            }else{
                node_stack[stack_index++] = nodes[node.node_child_index + 1];
                node_stack[stack_index++] = nodes[node.node_child_index];
            }
        }
    }
    return hit;
}
///////////////////////////////////////////////////////////////
