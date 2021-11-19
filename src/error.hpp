//
// Created by lpcvoid on 11/11/2021.
//


#pragma once

#include <cstdint>

namespace netlib {
    static std::error_condition socket_get_last_error(){
#ifdef _WIN32
        return WASGetLastError();
#else
        return std::make_error_condition(static_cast<std::errc>(errno));
#endif
    }
}

