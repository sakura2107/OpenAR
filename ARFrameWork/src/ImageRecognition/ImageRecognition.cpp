#include <ImageRecognition/ImageRecognition.h>
#include <ImageRecognition/MultiPointsRecognition.h>
#include <ImageRecognition/PrefixSumRecognition.h>
#if defined(ENABLE_CUDA)
#include <ImageRecognition/CudaMultiPointsRecognition.h>
#endif

std::unique_ptr<ar::ImageRecognition> ar::ImageRecognitionFactory::createIamgeRecognition(ar::imageRecognitionType type){
    switch(type){
        case(ar::imageRecognitionType::MPR):
            return std::make_unique<ar::MultiPointsRecognition>();
        case(ar::imageRecognitionType::PSR):
            return std::make_unique<ar::PrefixSumRecognition>();
        #if defined(ENABLE_CUDA)
        case(ar::imageRecognitionType::MPR_CUDA):
            return std::make_unique<ar::CudaMultiPointsRecognition>();
        #endif
    }
    //add other compare image method ...
    return nullptr;
}