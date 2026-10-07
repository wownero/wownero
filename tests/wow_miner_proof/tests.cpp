// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include "fixture.h"

#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>

namespace wr = wownero::research;
using wr::test::coinbase_fixture;
using wr::test::bytes;

TEST(WowMinerProof, CarrotCoinbaseReceiverCanSign)
{
    const coinbase_fixture f;
    EXPECT_TRUE(f.verify(f.sign()));
}

TEST(WowMinerProof, CarrotCoinbaseToLegacyAddressCanSign)
{
    const coinbase_fixture f(true);
    EXPECT_TRUE(f.verify(f.sign()));
    EXPECT_TRUE(sc_isnonzero(reinterpret_cast<const unsigned char *>(f.y.data)));
}

TEST(WowMinerProof, FreshNoncesForRepeatedStatement)
{
    const coinbase_fixture f;
    const auto first = f.sign();
    const auto second = f.sign();
    EXPECT_NE(first, second);
    EXPECT_TRUE(f.verify(first));
    EXPECT_TRUE(f.verify(second));
}

TEST(WowMinerProof, BindsEveryByteOfBlockAndNetworkDigest)
{
    coinbase_fixture f;
    const auto proof = f.sign();
    for (auto *digest : {&f.message, &f.network})
        for (auto &byte : digest->data)
        {
            byte ^= 1;
            EXPECT_FALSE(f.verify(proof));
            byte ^= 1;
        }
}

TEST(WowMinerProof, CannotTransferProofToAnotherCoinbase)
{
    const coinbase_fixture first, second;
    EXPECT_FALSE(second.verify(first.sign()));
}

TEST(WowMinerProof, RejectsMutationOfEveryProofByte)
{
    const coinbase_fixture f;
    auto proof = f.sign();
    for (auto &byte : proof)
    {
        byte ^= 1;
        EXPECT_FALSE(f.verify(proof));
        byte ^= 1;
    }
}

TEST(WowMinerProof, RejectsTruncatedAndExtendedProof)
{
    const coinbase_fixture f;
    const auto proof = f.sign();
    for (std::size_t length = 0; length < proof.size(); ++length)
        EXPECT_FALSE(wr::verify_miner_proof(f.network, f.message, f.enote.onetime_address,
            {proof.data(), length}));
    std::array<std::uint8_t, 97> extended{};
    std::copy(proof.begin(), proof.end(), extended.begin());
    EXPECT_FALSE(wr::verify_miner_proof(f.network, f.message, f.enote.onetime_address,
        {extended.data(), extended.size()}));
}

TEST(WowMinerProof, RejectsNoncanonicalScalarsAndZeroChallenge)
{
    const coinbase_fixture f;
    const auto original = f.sign();
    for (std::size_t offset : {0, 32, 64})
    {
        auto proof = original;
        std::fill(proof.begin() + offset, proof.begin() + offset + 32, 0xff);
        EXPECT_FALSE(f.verify(proof));
    }
    auto proof = original;
    std::fill(proof.begin(), proof.begin() + 32, 0);
    EXPECT_FALSE(f.verify(proof));
}

TEST(WowMinerProof, MissingEitherWitnessRefusesAndClearsOutput)
{
    const coinbase_fixture f;
    crypto::secret_key absent{};
    for (const bool missing_g : {false, true})
    {
        auto proof = f.sign();
        EXPECT_FALSE(wr::generate_miner_proof(f.network, f.message, f.enote.onetime_address,
            missing_g ? absent : f.x, missing_g ? f.y : absent, proof));
        EXPECT_TRUE(std::all_of(proof.begin(), proof.end(), [](auto byte) { return byte == 0; }));
    }
}

TEST(WowMinerProof, AccountKeysCannotSignWithoutCoinbaseExtensions)
{
    const coinbase_fixture f;
    wr::miner_proof proof;
    EXPECT_FALSE(wr::generate_miner_proof(f.network, f.message, f.enote.onetime_address,
        f.account_g, f.account_t, proof));
}

TEST(WowMinerProof, RejectsNoncanonicalSecretScalar)
{
    const coinbase_fixture f;
    crypto::secret_key invalid;
    std::memset(invalid.data, 0xff, sizeof(invalid.data));
    wr::miner_proof proof;
    EXPECT_FALSE(wr::generate_miner_proof(f.network, f.message, f.enote.onetime_address,
        invalid, f.y, proof));
    EXPECT_FALSE(wr::generate_miner_proof(f.network, f.message, f.enote.onetime_address,
        f.x, invalid, proof));
}

TEST(WowMinerProof, AllowsZeroComponentForValidNonidentityRepresentation)
{
    const coinbase_fixture f;
    const crypto::secret_key zero{};
    for (const bool zero_g : {false, true})
    {
        const auto &x = zero_g ? zero : f.x;
        const auto &y = zero_g ? f.y : zero;
        crypto::public_key key;
        carrot::make_carrot_spend_pubkey(x, y, key);
        wr::miner_proof proof;
        ASSERT_TRUE(wr::generate_miner_proof(f.network, f.message, key, x, y, proof));
        EXPECT_TRUE(wr::verify_miner_proof(f.network, f.message, key, {proof.data(), proof.size()}));
    }
}

TEST(WowMinerProof, RejectsIdentityTorsionMixedOrderAndNoncanonicalKeys)
{
    EXPECT_TRUE(wr::check_miner_public_key(crypto::get_G()));
    EXPECT_TRUE(wr::check_miner_public_key(crypto::get_T()));
    crypto::public_key identity{};
    identity.data[0] = 1;
    EXPECT_FALSE(wr::check_miner_public_key(identity));
    crypto::public_key order_two;
    std::memset(order_two.data, 0xff, 32);
    order_two.data[0] = static_cast<char>(0xec);
    order_two.data[31] = 0x7f;
    EXPECT_FALSE(wr::check_miner_public_key(order_two));

    ge_p3 torsion;
    ASSERT_EQ(0, ge_frombytes_vartime(&torsion, bytes(order_two)));
    ge_cached cached;
    ge_p3_to_cached(&cached, &torsion);
    ge_p1p1 mixed_sum;
    ge_add(&mixed_sum, &crypto::get_G_p3(), &cached);
    ge_p3 mixed_point;
    ge_p1p1_to_p3(&mixed_point, &mixed_sum);
    crypto::public_key mixed;
    ge_p3_tobytes(bytes(mixed), &mixed_point);
    EXPECT_FALSE(wr::check_miner_public_key(mixed));

    crypto::public_key noncanonical;
    std::memset(noncanonical.data, 0xff, 32);
    noncanonical.data[0] = static_cast<char>(0xee);
    noncanonical.data[31] = 0x7f;
    EXPECT_FALSE(wr::check_miner_public_key(noncanonical));
    identity.data[31] = static_cast<char>(0x80);
    EXPECT_FALSE(wr::check_miner_public_key(identity));

    coinbase_fixture f;
    const auto proof = f.sign();
    for (const auto &key : {identity, order_two, mixed, noncanonical})
    {
        f.enote.onetime_address = key;
        EXPECT_FALSE(f.verify(proof));
    }
}
