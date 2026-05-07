// Vendor
#include <cuda_runtime.h>

struct PushConstant;

void launch_trace_world(dim3 &block, dim3 &grid, cudaStream_t cu_stream,
                        cudaTextureObject_t output_image_read,
                        cudaSurfaceObject_t output_image_write,
                        const int image_width, const int image_height);

void launch_trace_world(PushConstant &constant_data, cudaStream_t cu_stream,
                        void *output_buffer);
