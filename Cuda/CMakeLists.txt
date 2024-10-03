cuda_add_library(CudaMatch SHARED ${PROJECT_SOURCE_DIR}/Cuda/src/TemplateMatchMPRCuda.cu)

target_include_directories(CudaMatch PRIVATE ${PROJECT_SOURCE_DIR}/Cuda/include)

set_target_properties(CudaMatch PROPERTIES
    CXX_STANDARD 17
    CUDA_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CUDA_STANDARD_REQUIRED YES
)