// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "carrot_core/payment_proposal.h"
#include "crypto/generators.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/wow_miner.h"
#include "string_tools.h"
#include "wownero/testnet_genesis.h"

namespace
{
using namespace cryptonote;

struct block_fixture
{
    wow_miner_account account;
    crypto::secret_key x, y;
    block b{};

    explicit block_fixture(bool legacy = false)
    {
        crypto::random32_unbiased(reinterpret_cast<unsigned char *>(account.generate_image.data));
        if (!legacy)
            crypto::random32_unbiased(reinterpret_cast<unsigned char *>(account.prove_spend.data));
        crypto::random32_unbiased(reinterpret_cast<unsigned char *>(account.view_incoming.data));
        account_public_address address;
        if (!account.address(address))
            throw std::runtime_error("Invalid generated test account");
        carrot::CarrotPaymentProposalV1 proposal{};
        carrot::make_carrot_main_address_v1(address.m_spend_public_key, address.m_view_public_key, proposal.destination);
        proposal.amount = COIN;
        proposal.randomness = carrot::gen_janus_anchor();
        carrot::CarrotCoinbaseEnoteV1 enote;
        carrot::get_coinbase_enote_v1(proposal, 101, enote);
        b.major_version = HF_VERSION_CARROT;
        b.minor_version = HF_VERSION_CARROT;
        b.timestamp = 1000;
        b.nonce = 7;
        b.fcmp_pp_n_tree_layers = 1;
        b.fcmp_pp_tree_root = {};
        auto &tx = b.miner_tx;
        tx.version = 2;
        tx.unlock_time = 101 + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
        tx.vin.emplace_back(txin_gen{101});
        txout_to_carrot_v1 output;
        output.key = enote.onetime_address;
        output.view_tag = enote.view_tag;
        output.encrypted_janus_anchor = enote.anchor_enc;
        tx.vout.push_back({proposal.amount, output});
        crypto::public_key ephemeral;
        std::memcpy(&ephemeral, &enote.enote_ephemeral_pubkey, sizeof(ephemeral));
        add_tx_pub_key_to_extra(tx, ephemeral);
        if (!prepare_wow_miner_output_keys(b, account, x, y) || !sign_wow_miner_block(b, x, y))
            throw std::runtime_error("Unable to prepare signed Carrot test block");
    }
};

TEST(WowTestnet, NewAndLegacyCarrotAccounts)
{
    for (bool legacy : {false, true})
    {
        block_fixture fixture(legacy);
        EXPECT_TRUE(verify_wow_miner_block(fixture.b));
        auto wrong = fixture.account;
        crypto::random32_unbiased(reinterpret_cast<unsigned char *>(wrong.view_incoming.data));
        EXPECT_FALSE(prepare_wow_miner_output_keys(fixture.b, wrong, fixture.x, fixture.y));
    }
}

TEST(WowTestnet, CompleteBlockBinding)
{
    block_fixture fixture;
    const std::function<void(block &)> mutations[] = {
        [](block &b) { ++b.nonce; },
        [](block &b) { ++b.timestamp; },
        [](block &b) { ++b.minor_version; },
        [](block &b) { b.prev_id.data[0] ^= 1; },
        [](block &b) { b.wow_vote = 1; },
        [](block &b) { ++b.miner_tx.unlock_time; },
        [](block &b) { ++b.miner_tx.vout.front().amount; },
        [](block &b) { b.miner_tx.extra.push_back(0); },
        [](block &b) { ++boost::get<txin_gen>(b.miner_tx.vin.front()).height; },
        [](block &b) { b.tx_hashes.push_back(crypto::cn_fast_hash("tx", 2)); },
        [](block &b) { ++b.fcmp_pp_n_tree_layers; },
        [](block &b) { b.fcmp_pp_tree_root.data[0] ^= 1; },
    };
    // Populate caches first; verification must not trust stale cached hashes.
    get_block_hash(fixture.b);
    for (const auto &mutate : mutations)
    {
        block altered = fixture.b;
        mutate(altered);
        EXPECT_FALSE(verify_wow_miner_block(altered));
    }
}

TEST(WowTestnet, SignatureSerializationAndPowBinding)
{
    block_fixture fixture;
    block decoded;
    ASSERT_TRUE(parse_and_validate_block_from_blob(block_to_blob(fixture.b), decoded));
    EXPECT_TRUE(verify_wow_miner_block(decoded));
    const auto statement = get_wow_miner_statement_hash(fixture.b);
    const auto pow_blob = get_block_hashing_blob(fixture.b);
    fixture.b.wow_miner_signature[0] ^= 1;
    EXPECT_EQ(statement, get_wow_miner_statement_hash(fixture.b));
    EXPECT_NE(pow_blob, get_block_hashing_blob(fixture.b));
    EXPECT_FALSE(verify_wow_miner_block(fixture.b));
}

TEST(WowTestnet, RejectsInvalidMinerShapes)
{
    block_fixture fixture;
    for (auto vote : {0, 1, 2})
    {
        fixture.b.wow_vote = vote;
        ASSERT_TRUE(sign_wow_miner_block(fixture.b, fixture.x, fixture.y));
        EXPECT_TRUE(verify_wow_miner_block(fixture.b));
    }
    fixture.b.wow_vote = 3;
    EXPECT_FALSE(sign_wow_miner_block(fixture.b, fixture.x, fixture.y));
    fixture.b.wow_vote = 0;
    fixture.b.miner_tx.vout.push_back(fixture.b.miner_tx.vout.front());
    EXPECT_FALSE(sign_wow_miner_block(fixture.b, fixture.x, fixture.y));
    fixture.b.miner_tx.vout.resize(1);
    fixture.b.miner_tx.vout.front().target = txout_to_key{};
    EXPECT_FALSE(verify_wow_miner_block(fixture.b));
}

TEST(WowTestnet, IsolatedNetworkAndGenesis)
{
    EXPECT_THROW(get_config(MAINNET), std::runtime_error);
    EXPECT_THROW(get_config(STAGENET), std::runtime_error);
    EXPECT_THROW(get_config(FAKECHAIN), std::runtime_error);
    EXPECT_NO_THROW(get_config(TESTNET));
    block genesis{};
    genesis.major_version = 1;
    genesis.nonce = wownero::testnet::genesis_nonce;
    std::string blob;
    ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(wownero::testnet::genesis_tx, blob));
    ASSERT_TRUE(parse_and_validate_tx_from_blob(blob, genesis.miner_tx));
    EXPECT_EQ(epee::string_tools::pod_to_hex(get_block_hash(genesis)), wownero::testnet::genesis_hash);
    EXPECT_EQ(genesis.miner_tx.unlock_time, CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);
    EXPECT_TRUE(wownero::research::check_miner_public_key(
        boost::get<txout_to_key>(genesis.miner_tx.vout.front().target).key));
}

TEST(WowTestnet, MiningCredentialsUsePipeAndCloseDescriptor)
{
    block_fixture fixture;
    int descriptors[2];
    ASSERT_EQ(pipe(descriptors), 0);
    ASSERT_EQ(write(descriptors[1], "WOWMIN01", 8), 8);
    for (const auto *key : {&fixture.account.generate_image, &fixture.account.prove_spend, &fixture.account.view_incoming})
        ASSERT_EQ(write(descriptors[1], key->data, 32), 32);
    close(descriptors[1]);
    wow_miner_account decoded;
    ASSERT_TRUE(read_wow_miner_account(descriptors[0], decoded));
    EXPECT_EQ(fcntl(descriptors[0], F_GETFD), -1);
    account_public_address expected, actual;
    ASSERT_TRUE(fixture.account.address(expected));
    ASSERT_TRUE(decoded.address(actual));
    EXPECT_TRUE(expected == actual);

    ASSERT_EQ(pipe(descriptors), 0);
    ASSERT_EQ(write(descriptors[1], "WOWMIN01", 8), 8);
    close(descriptors[1]);
    EXPECT_FALSE(read_wow_miner_account(descriptors[0], decoded));
    EXPECT_FALSE(decoded.address(actual));
    EXPECT_EQ(fcntl(descriptors[0], F_GETFD), -1);
    EXPECT_FALSE(read_wow_miner_account(STDIN_FILENO, decoded));
}
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
