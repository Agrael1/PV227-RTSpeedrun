#pragma once
#include <wisdom/wisdom.hpp>

namespace w {
static constexpr wis::DataFormat swap_format = wis::DataFormat::RGBA8Unorm; // standard format for the application
static constexpr wis::DataFormat depth_format = wis::DataFormat::D32Float; // standard format for the application
static constexpr uint32_t swap_frames = 2; // 
static constexpr uint32_t flight_frames = 2; //

struct Exception : public std::exception {
    Exception(std::string message)
        : message(std::move(message))
    {
    }

    const char* what() const noexcept override
    {
        return message.c_str();
    }

    std::string message;
};

inline void CheckResult(wis::Result res)
{
    if (res.status != wis::Status::Ok) {
        throw Exception(res.error);
    }
}
}