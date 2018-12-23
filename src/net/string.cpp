

#include "string.h"

#include "net/tor_address.h"
#include "string_tools.h"

namespace net
{
    expect<epee::net_utils::network_address>
    get_network_address(const boost::string_ref address, const std::uint16_t default_port)
    {
        const boost::string_ref host = address.substr(0, address.rfind(':'));

        if (host.empty())
            return make_error_code(net::error::kInvalidHost);
        if (host.ends_with(".onion"))
            return tor_address::make(address, default_port);
        if (host.ends_with(".i2p"))
            return make_error_code(net::error::kInvalidI2PAddress); // not yet implemented (prevent public DNS lookup)

        std::uint16_t port = default_port;
        if (host.size() < address.size())
        {
            if (!epee::string_tools::get_xtype_from_string(port, std::string{address.substr(host.size() + 1)}))
                return make_error_code(net::error::kInvalidPort);
        }

        std::uint32_t ip = 0;
        if (epee::string_tools::get_ip_int32_from_string(ip, std::string{host}))
            return {epee::net_utils::ipv4_network_address{ip, port}};
        return make_error_code(net::error::kUnsupportedAddress);
    }
}
