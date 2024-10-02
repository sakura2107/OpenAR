set(PCR_IMG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/res)
set(PCR_IMG_DST_DIR ${CMAKE_BINARY_DIR}/PCRDemo/res)
set(EXTERNAL_RENDERER_IPC_Dll_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/bin/MuMu)
set(EXTERNAL_RENDERER_IPC_Dll_DST_DIR ${CMAKE_BINARY_DIR}/PCRDemo)
set(ADB_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/bin/adb)
set(ADB_DST_DIR ${CMAKE_BINARY_DIR}/PCRDemo/adb)
set(TESSERACT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/TesseractOCRData)
set(TESSERACT_DST_DIR ${CMAKE_BINARY_DIR}/PCRDemo/TesseractOCRData)

add_library(PCRLib STATIC ${PROJECT_SOURCE_DIR}/PCRDemo/src/Init.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/LaunchDevice.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/Login.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/CloseDevice.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/ExecuteSchedule.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/ReceiveGifts.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/MainLineNormal.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/MainLineVeryHard.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/Activity.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/MainLineHard.cpp
                          ${PROJECT_SOURCE_DIR}/PCRDemo/src/Store.cpp)

target_include_directories(PCRLib PRIVATE ${PROJECT_SOURCE_DIR}/FrameWork/include
                                          ${PROJECT_SOURCE_DIR}/PCRDemo/include
                                          ${OpenCV_INCLUDE_DIRS})

target_link_libraries(PCRLib PRIVATE FrameWork ${OpenCV_LIBS})

add_executable(main ${PROJECT_SOURCE_DIR}/PCRDemo/src/main.cpp)

target_include_directories(main PRIVATE ${PROJECT_SOURCE_DIR}/FrameWork/include
                                        ${PROJECT_SOURCE_DIR}/PCRDemo/include
                                        ${OpenCV_INCLUDE_DIRS})

target_link_libraries(main PRIVATE PCRLib ${OpenCV_LIBS} FrameWork)

add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${EXTERNAL_RENDERER_IPC_Dll_SOURCE_DIR} ${EXTERNAL_RENDERER_IPC_Dll_DST_DIR}
    COMMENT "Copying DLLs to executable directory"
)

add_custom_command(
    TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PCR_IMG_SOURCE_DIR} ${PCR_IMG_DST_DIR}
    COMMENT "Copying img folder to build directory"
)

add_custom_command(
    TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${ADB_SOURCE_DIR} ${ADB_DST_DIR}
    COMMENT "Copying ADB folder to build directory"
)

add_custom_command(
    TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${TESSERACT_SOURCE_DIR} ${TESSERACT_DST_DIR}
    COMMENT "Copying Tesseract ocr folder to build directory"
)