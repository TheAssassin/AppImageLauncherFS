#pragma once

// system includes
#include <stdexcept>

class AlreadyRunningError : public std::runtime_error {
public:
    explicit AlreadyRunningError(const std::string& msg) : runtime_error(msg) {}
};
