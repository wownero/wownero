// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <boost/algorithm/string.hpp>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "net/parse.h"

namespace nodetool
{
  // TXT seed records contain literal IPv4 endpoints, separated by semicolons.
  inline std::set<std::string> parse_dns_seed_records(const std::vector<std::string>& records, uint16_t default_port)
  {
    std::set<std::string> seeds;
    for (const auto& record : records)
    {
      std::vector<std::string> endpoints;
      boost::split(endpoints, record, boost::is_any_of(";"));
      for (auto endpoint : endpoints)
      {
        boost::trim(endpoint);
        const auto address = net::get_network_address(endpoint, default_port);
        if (!address || address->get_type_id() != epee::net_utils::address_type::ipv4)
          continue;
        const auto& ipv4 = address->as<epee::net_utils::ipv4_network_address>();
        if (ipv4.ip() == 0 || ipv4.port() == 0 || ipv4.is_loopback() || ipv4.is_local())
          continue;
        seeds.insert(address->str());
      }
    }
    return seeds;
  }

}
