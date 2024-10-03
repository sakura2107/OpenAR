add_library(FrameWork STATIC ${PROJECT_SOURCE_DIR}/FrameWork/src/MuMuController.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/TemplateMatchMPR.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/MultiPointsRecognition.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/DeviceController.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/MuMuDeviceController.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/Controller.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/ImageRecognition.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/MiniLog.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/AdbController.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/TextRecognition.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/TesseractOCR.cpp
                             ${PROJECT_SOURCE_DIR}/FrameWork/src/TextMatchTesseractOCR.cpp)

target_include_directories(FrameWork PRIVATE ${PROJECT_SOURCE_DIR}/FrameWork/include
                                                ${PROJECT_SOURCE_DIR}/3rdparty/include
                                                ${OpenCV_INCLUDE_DIRS}
                                                ${Tesseract_INCLUDE_DIRS})

if(ENABLE_CUDA)
    target_include_directories(FrameWork PRIVATE ${PROJECT_SOURCE_DIR}/Cuda/include)
    target_link_libraries(FrameWork PRIVATE ${OpenCV_LIBS} Tesseract::libtesseract CudaMatch ${PROJECT_SOURCE_DIR}/FrameWork/src/CudaMultiPointsRecognition.cpp)
else()
    target_link_libraries(FrameWork PRIVATE ${OpenCV_LIBS})
endif()