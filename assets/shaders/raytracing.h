// Structs
struct Ray{
    vec3 origin;
    vec3 direction;
};

struct Material{
    vec3 albedo;
    bool metal;
};

struct Sphere{
    vec3 origin;
    float radius;
    Material mat;
};

struct HitRecord {
    vec3 position;
    vec3 normal;
    Material mat;
    float t; // Why need t if you already have the position?
    bool front_face;
};


// Linear to Gamma
float linear_to_gamma(float linear_component)
{
    if (linear_component > 0)
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
bool near_zero(vec3 v) {
    // Return true if the vector is close to zero in all dimensions.
    float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

void scatter_lambertian(inout uint seed, Material mat, const Ray r_in, const HitRecord rec, out vec3 attenuation, out Ray scattered){
    vec3 scatter_direction = rec.normal + rand_unit_vector(seed);
    if(near_zero(scatter_direction))
        scatter_direction = rec.normal;

    scattered = Ray(rec.position, scatter_direction);
    attenuation = mat.albedo;
}

void scatter_metal(inout uint seed, Material mat, const Ray r_in, const HitRecord rec, out vec3 attenuation, out Ray scattered){
    vec3 reflected_direction = reflect(r_in.direction, rec.normal);
    scattered = Ray(rec.position, reflected_direction);
    attenuation = mat.albedo;
}

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

    hit_record.front_face = dot(ray.direction, outward_normal) < 0.0f;
    hit_record.normal = hit_record.front_face ? outward_normal : -outward_normal;

    hit_record.t = t;
    hit_record.mat = sphere.mat;

    return true;
}


// Spheres
const Material materials[4] = Material[4](
    Material(vec3(0.8, 0.8, 0.0), false),
    Material(vec3(0.1, 0.2, 0.5), false),
    Material(vec3(0.8, 0.8, 0.8), true),
    Material(vec3(0.8, 0.6, 0.2), true)
);


#define SPHERE_COUNT 4
const Sphere world[SPHERE_COUNT] = Sphere[SPHERE_COUNT](
    Sphere(vec3(-2.f, 0.f, -3.f), 1.f, materials[2]), // Left
    Sphere(vec3(0.f, 0.f, -3.f), 1.f, materials[1]), // Centre
    Sphere(vec3(2.f, 0.f, -3.f), 1.f, materials[3]), // Right
    Sphere(vec3(0.f, -101.f, -3.f), 100.f, materials[0]) // Ground
);

bool hit_world(Ray ray, float ray_tmin, float ray_tmax, inout HitRecord hit_record){
    HitRecord temp_rec;
    bool hit_anything = false;
    float closest_t_so_far = ray_tmax;

    for(int i = 0; i < world.length(); i++){
        if(hit_sphere(ray, ray_tmin, ray_tmax, temp_rec, world[i])){
            hit_anything = true;
            if(temp_rec.t < closest_t_so_far){
                closest_t_so_far = temp_rec.t;
                hit_record = temp_rec;
            }
            
        }
    }

    return hit_anything;
}
