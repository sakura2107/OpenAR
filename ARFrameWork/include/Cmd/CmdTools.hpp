#pragma once
#include <string>
#include <array>
#include <memory>
namespace ar{

inline void exec_cmd(const std::string& cmd, std::string& cmd_res){
    std::array<char,128> buffer;

    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
    while(fgets(buffer.data(), (int)buffer.size(), pipe.get()) != nullptr) {
        cmd_res += buffer.data();
    }
}

}