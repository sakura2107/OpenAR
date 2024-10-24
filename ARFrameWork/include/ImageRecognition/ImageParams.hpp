#pragma once

namespace ar{

struct point{
    int x = 0;
    int y = 0;
    unsigned char data = 0;
    bool is_empty = true;
};

enum struct imageRecognitionType{
    MPR,
    PSR,
    #if defined(ENABLE_CUDA)
    MPR_CUDA,
    #endif
};

}