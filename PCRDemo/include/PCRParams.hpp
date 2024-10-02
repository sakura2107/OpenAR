#pragma once

namespace pcr{

enum class image_recognition_type: int {
    mpr = 0,            //multi_point_recognition           T
    orb = 1,            //orb_recognition                   T
    tpc = 2,            //template_compare                  F
    tpc_gpu = 3,        //template_compare with gpu(cuda)   F
};

enum class controller_type: int {
    mumu_api = 0,       //mumu_api                          T
    mini_touch = 1,     //minitouch and minicap             F
};

}