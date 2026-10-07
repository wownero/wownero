// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include <iostream>
#include <string_view>

#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "string_tools.h"

namespace
{
crypto::public_key public_point(std::string_view domain)
{
    crypto::public_key key;
    crypto::unbiased_hash_to_ec(reinterpret_cast<const unsigned char *>(domain.data()), domain.size(), key);
    return key;
}
}

int main()
{
    cryptonote::block genesis{};
    genesis.major_version = 1;
    genesis.minor_version = 0;
    genesis.timestamp = 0;
    genesis.nonce = 20261004;
    auto &tx = genesis.miner_tx;
    tx.version = 1;
    tx.unlock_time = 288;
    tx.vin.emplace_back(cryptonote::txin_gen{0});
    // Hash-to-point creates a genesis destination without a known spending witness.
    tx.vout.push_back({UINT64_MAX >> 20,
        cryptonote::txout_to_key{public_point("Wownero FCMP testnet genesis output 2026-10-04")}});
    cryptonote::add_tx_pub_key_to_extra(tx,
        public_point("Wownero FCMP testnet genesis transaction key 2026-10-04"));
    cryptonote::add_extra_nonce_to_tx_extra(tx.extra, "WOW FCMP++ testnet 2026-10-04");
    std::cout << "// Copyright (c) 2026, The Wownero Project\n"
        << "// SPDX-License-Identifier: BSD-3-Clause\n"
        << "// Do not edit. Regenerate with the wow_testnet_genesis executable.\n\n"
        << "#pragma once\n\nnamespace wownero::testnet\n{\n"
        << "inline constexpr char genesis_tx[] = \""
        << epee::string_tools::buff_to_hex_nodelimer(cryptonote::tx_to_blob(tx)) << "\";\n"
        << "inline constexpr char genesis_hash[] = \""
        << epee::string_tools::pod_to_hex(cryptonote::get_block_hash(genesis)) << "\";\n"
        << "inline constexpr unsigned genesis_nonce = 20261004;\n}\n";
}
