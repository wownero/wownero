// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "cryptonote_basic.h"

namespace cryptonote
{
struct wow_miner_account
{
    crypto::secret_key generate_image{}, prove_spend{}, view_incoming{};
    bool address(account_public_address &out) const;
};

// Consumes a pipe descriptor and closes it on every outcome. The frame is
// "WOWMIN01" followed by three canonical scalars; no credential enters argv.
bool read_wow_miner_account(int fd, wow_miner_account &out);

crypto::hash get_wow_miner_statement_hash(const block &b);
bool prepare_wow_miner_output_keys(const block &b, const wow_miner_account &account,
    crypto::secret_key &x, crypto::secret_key &y);
bool sign_wow_miner_block(block &b, const crypto::secret_key &x, const crypto::secret_key &y);
bool verify_wow_miner_block(const block &b);
}
