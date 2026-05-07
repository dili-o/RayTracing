#define CUDA_CHECK(call)                                                       \
  {                                                                            \
    const cudaError_t result_ = call;                                          \
    HASSERT_MSGS(result_ == cudaSuccess, "Error: {}",                          \
                 cudaGetErrorString(result_));                                 \
  }
