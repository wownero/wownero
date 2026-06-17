#pragma once

#include <stdexcept>

namespace hw
{
    namespace error
    {
        struct device_disconnected : public std::runtime_error
        {
            explicit device_disconnected(const std::string& message)
                : std::runtime_error("Device disconnected: " + message)
            {
            }
        };

        template<typename TException>
        void throw_exception(const std::string &msg)
        {
            TException e(msg);
            LOG_PRINT_L0(e.what());
            throw e;
        }
    }
}
