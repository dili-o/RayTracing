// Structs
struct Ray{
    vec3 direction;
    vec3 origin;
};

struct Sphere{
    vec3 origin;
    float radius;
};

struct HitRecord {
    vec3 position;
    vec3 normal;
    float t; // Why need t if you already have the position?
    bool front_face;
};

// Functions
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

    return true;
}

// 0 to 1
float rand(vec2 coord) {
    return fract(sin(dot(coord, vec2(12.9898, 78.233))) * 43758.5453123);
}

float randInRange(vec2 coord, float min, float max) {
    return min + (max - min) * rand(coord);
}

// Spheres
#define SPHERE_COUNT 3
const Sphere world[SPHERE_COUNT] = Sphere[SPHERE_COUNT](
    Sphere(vec3(0.f, 0.f, -2.f), 0.5f),
    Sphere(vec3(0.f, -105.5f, -1.f), 100.f),
    Sphere(vec3(1.5f, 0.0f, -2.f), 1.f)
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
