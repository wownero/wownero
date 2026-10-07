// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "crypto.h"
#include "span.h"

namespace wownero::research
{
// Experimental Schnorr representation proof for K = xG + yT.
using miner_proof = std::array<std::uint8_t, 96>;

bool check_miner_public_key(const crypto::public_key &key);

// block_digest must cover the complete canonical block statement, excluding the proof itself.
// network_genesis is the network's genesis hash. Both parameters are transcript commitments.
// Returns false and clears proof on invalid scalars, public key, or witness mismatch.
bool generate_miner_proof(const crypto::hash &network_genesis,
    const crypto::hash &block_digest, const crypto::public_key &key,
    const crypto::secret_key &x, const crypto::secret_key &y, miner_proof &proof);

bool verify_miner_proof(const crypto::hash &network_genesis,
    const crypto::hash &block_digest, const crypto::public_key &key,
    epee::span<const std::uint8_t> proof);
}
