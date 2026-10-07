// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "carrot_core/account_secrets.h"
#include "carrot_core/device_ram_borrowed.h"
#include "carrot_core/payment_proposal.h"
#include "carrot_core/scan.h"
#include "crypto/generators.h"
#include "crypto/wow_miner_proof.h"

#include <stdexcept>

namespace wownero::research::test
{
template<class T> unsigned char *bytes(T &value)
{
    return reinterpret_cast<unsigned char *>(value.data);
}

struct coinbase_fixture
{
    crypto::secret_key account_g, account_t, x, y;
    carrot::CarrotCoinbaseEnoteV1 enote;
    crypto::hash network = crypto::cn_fast_hash("research-network", 16);
    crypto::hash message = crypto::cn_fast_hash("research-block", 14);

    explicit coinbase_fixture(bool legacy_address = false)
    {
        crypto::secret_key master, view_balance, image_preimage, view;
        crypto::random32_unbiased(bytes(master));
        crypto::public_key partial_spend, account_spend, view_public;
        carrot::make_carrot_provespend_key(master, account_t);
        carrot::make_carrot_partial_spend_pubkey(account_t, partial_spend);
        carrot::make_carrot_viewbalance_secret(master, view_balance);
        carrot::make_carrot_generateimage_preimage(view_balance, image_preimage);
        carrot::make_carrot_generateimage_key(image_preimage, partial_spend, account_g);
        carrot::make_carrot_viewincoming_key(view_balance, view);
        if (legacy_address)
            account_t = crypto::secret_key{};
        carrot::make_carrot_spend_pubkey(account_g, account_t, account_spend);
        if (!crypto::secret_key_to_public_key(view, view_public))
            throw std::runtime_error("Unable to derive fixture view public key");

        carrot::CarrotPaymentProposalV1 proposal{};
        carrot::make_carrot_main_address_v1(account_spend, view_public, proposal.destination);
        proposal.amount = 100000000000;
        proposal.randomness = carrot::gen_janus_anchor();
        carrot::get_coinbase_enote_v1(proposal, 101, enote);
        carrot::view_incoming_key_ram_borrowed_device device(view);
        mx25519_pubkey shared;
        crypto::secret_key extension_g, extension_t;
        if (!carrot::try_make_carrot_shared_key_receiver(device, enote.enote_ephemeral_pubkey, shared) ||
            !carrot::try_scan_carrot_coinbase_enote_receiver(enote, shared, account_spend,
                view_public, extension_g, extension_t))
            throw std::runtime_error("Unable to scan fixture coinbase");

        sc_add(bytes(x), bytes(account_g), bytes(extension_g));
        sc_add(bytes(y), bytes(account_t), bytes(extension_t));
    }

    miner_proof sign() const
    {
        miner_proof proof;
        if (!generate_miner_proof(network, message, enote.onetime_address, x, y, proof))
            throw std::runtime_error("Unable to sign fixture coinbase");
        return proof;
    }

    bool verify(const miner_proof &proof) const
    {
        return verify_miner_proof(network, message, enote.onetime_address,
            {proof.data(), proof.size()});
    }
};
}
