#include <iostream>
#include <string>
#include <Init.h>
#include <LaunchDevice.h>
#include <CloseDevice.h>
#include <Login.h>
#include <ExecuteSchedule.h>
#include <ReceiveGifts.h>
#include <MainLineNormal.h>
#include <Store.h>
#include <Log/MiniLog.h>
#include <TextRecognition/TesseractOCR.h>

int main(){
    ar::setMiniLogLevel(ar::level::debug);
    const std::string mumu_path = "D:\\MuMu\\MuMuPlayer-12.0";
    int index = 1;
    
    pcr::global_params params(mumu_path,
                              index, 
                              "127.0.0.1",
                              16416,
                              ar::controllerType::MUMU, 
                              #if defined(ENABLE_CUDA)
                              ar::imageRecognitionType::MPR,
                              #else
                              ar::imageRecognitionType::MPR,
                              #endif
                              ar::deviceControllerType::MUMU);
    
    params.start_with_silence = false;
    params.operate_duration = 200;

    //Compare image speed
    pcr::checkImageCompareRate(ar::imageRecognitionType::MPR);

    //task
    pcr::launchDevice(params);
    pcr::login(params);
    pcr::execute_schedule(params);
    pcr::receive_gifts(params);
    pcr::main_line_normal_params normal_params;
    normal_params.is_auto_execute_in_multi_drop = true;
    pcr::main_line_normal(params, normal_params);
    pcr::store_params s_params;
    s_params.is_buy_lotion = true;
    s_params.is_buy_refining_stone = true;
    pcr::Store(params, s_params);
    return 0;
}