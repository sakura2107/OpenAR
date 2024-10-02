#pragma once
#include <vector>
#include <string>

#ifdef _WIN32
    #ifdef CudaMatch_EXPORTS
        #define CUDAMATCH_API __declspec(dllexport)
    #else
        #define CUDAMATCH_API __declspec(dllimport)
    #endif
#else
    #define CUDAMATCH_API
#endif

namespace ar {

bool CUDAMATCH_API templateMatchSSDCuda(int* res,
                          std::string& res_msg,
                          const unsigned char* image,
                          const unsigned char* temp,
                          const int& image_width,
                          const int& image_height,
                          const int& temp_width,
                          const int& temp_height,
                          const float& threshold,
                          const int& num_points);

//division 8 => 2 x 4   16 => 4 x 4   32 => 4 x 8
void findBestDivision(int*best_division, const int& num_points);

void makePointsRandom(int* division_random_point, const int& num_points, const int& temp_width, const int& temp_height);

}