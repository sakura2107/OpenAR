#pragma once

namespace ar{

struct point{
    int x = -1;
    int y = -1;
    unsigned char data = 0;
    bool is_empty = true;
};

enum struct imageRecognitionType{
    MPR,
    #if defined(ENABLE_CUDA)
    MPR_CUDA,
    #endif
};

}