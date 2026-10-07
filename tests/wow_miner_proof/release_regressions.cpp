// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <limits>
#include <thread>

#include "blockchain_db/lmdb/db_lmdb.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_core.h"
#include "easylogging++.h"
#include "rpc/rpc_handler.h"
#include "string_tools.h"
#include "wownero/testnet_genesis.h"

namespace
{
using namespace cryptonote;
using namespace std::chrono_literals;

TEST(WowReleaseRegression, LegacyRingOffsetsRespectForkBoundary)
{
    transaction tx;
    tx.version = 2;
    txin_to_key input{};
    input.key_offsets = {std::numeric_limits<uint64_t>::max(), 1};
    tx.vin.push_back(input);
    EXPECT_TRUE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS - 1));
    EXPECT_FALSE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS));
    EXPECT_FALSE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS + 1));

    auto &ring = boost::get<txin_to_key>(tx.vin.front()).key_offsets;
    ring = {std::numeric_limits<uint64_t>::max() - 1, 1};
    EXPECT_TRUE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS));
    ring = {1, std::numeric_limits<uint64_t>::max()};
    EXPECT_FALSE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS));
}

TEST(WowReleaseRegression, LegacyRingOffsetsAreCheckedPerInput)
{
    transaction tx;
    tx.version = 2;
    txin_to_key first{}, second{};
    first.key_offsets = {std::numeric_limits<uint64_t>::max()};
    second.key_offsets = {1, 2};
    tx.vin = {first, second};
    EXPECT_TRUE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS));
    boost::get<txin_to_key>(tx.vin.back()).key_offsets = {std::numeric_limits<uint64_t>::max(), 2};
    EXPECT_FALSE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS));
    tx.vin.back() = txin_gen{};
    EXPECT_FALSE(core::check_tx_inputs_ring_members_overflow(tx, HF_VERSION_FCMP_PLUS_PLUS));
}

TEST(WowReleaseRegression, PrunedBaseRequiresExactBoundary)
{
    std::string blob;
    ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(wownero::testnet::genesis_tx, blob));
    transaction tx;
    ASSERT_TRUE(parse_and_validate_tx_base_from_blob(blob, tx, true, true));
    blob.push_back('\0');
    EXPECT_FALSE(parse_and_validate_tx_base_from_blob(blob, tx, true, true));
    EXPECT_TRUE(parse_and_validate_tx_base_from_blob(blob, tx, true, false));
    blob.pop_back();
    blob.pop_back();
    EXPECT_FALSE(parse_and_validate_tx_base_from_blob(blob, tx, true, true));
}

TEST(WowReleaseRegression, NestedDatabaseAdmissionSurvivesClosedGate)
{
    const auto baseline = mdb_txn_safe::num_active_txns.load();
    std::promise<void> outer_ready, start_nested, nested_done, release_outer;
    auto ready = outer_ready.get_future();
    auto start = start_nested.get_future();
    auto done = nested_done.get_future();
    auto release = release_outer.get_future();
    std::thread worker([&] {
        mdb_txn_safe outer;
        outer_ready.set_value();
        start.wait();
        {
            mdb_txn_safe nested;
            nested_done.set_value();
            release.wait();
        }
    });
    ready.wait();
    mdb_txn_safe::prevent_new_txns();
    start_nested.set_value();
    const bool nested_admitted = done.wait_for(2s) == std::future_status::ready;
    // Reopen even on failure so the regression cannot strand its worker.
    mdb_txn_safe::allow_new_txns();
    release_outer.set_value();
    worker.join();
    EXPECT_TRUE(nested_admitted);
    EXPECT_EQ(mdb_txn_safe::num_active_txns.load(), baseline);
}

TEST(WowReleaseRegression, FreshDatabaseAdmissionWaitsForOpenGate)
{
    const auto baseline = mdb_txn_safe::num_active_txns.load();
    std::promise<void> started, admitted;
    auto running = started.get_future();
    auto ready = admitted.get_future();
    mdb_txn_safe::prevent_new_txns();
    std::thread worker([&] {
        started.set_value();
        mdb_txn_safe txn;
        admitted.set_value();
    });
    running.wait();
    const bool remained_closed = ready.wait_for(100ms) == std::future_status::timeout;
    mdb_txn_safe::allow_new_txns();
    worker.join();
    EXPECT_TRUE(remained_closed);
    EXPECT_EQ(mdb_txn_safe::num_active_txns.load(), baseline);
}

TEST(WowReleaseRegression, DistributionRefusalPreservesCachedBounds)
{
    bool reorganized = false;
    auto hash = [&](uint64_t height) {
        crypto::hash value{};
        value.data[0] = static_cast<char>(height);
        value.data[1] = reorganized && height > 10;
        return value;
    };
    unsigned calls = 0;
    auto distribution = [&](uint64_t, uint64_t, uint64_t,
        uint64_t &start, std::vector<uint64_t> &values, uint64_t &base) {
        ++calls;
        start = 16;
        base = 0;
        values = {1, 2, 3, 4, 5};
        return true;
    };
    ASSERT_TRUE(rpc::RpcHandler::get_output_distribution(distribution, 0, 0, 20, hash, true, 40));
    reorganized = true;
    EXPECT_FALSE(rpc::RpcHandler::get_output_distribution(distribution, 0, 0, 21, hash, true, 40));
    EXPECT_FALSE(rpc::RpcHandler::get_output_distribution(distribution, 0, 0, 21, hash, true, 40));
    EXPECT_EQ(calls, 1u);
    reorganized = false;
    const auto original = rpc::RpcHandler::get_output_distribution(distribution, 0, 0, 20, hash, true, 40);
    ASSERT_TRUE(original);
    EXPECT_EQ(original->distribution, (std::vector<uint64_t>{1, 2, 3, 4, 5}));
}

TEST(WowReleaseRegression, ConsoleSanitizationRemovesEscapeControls)
{
    std::string message = "payment \x1b[2J accepted";
    el::base::sanitize(message);
    EXPECT_EQ(message.find('\x1b'), std::string::npos);
    EXPECT_NE(message.find("payment"), std::string::npos);
    EXPECT_NE(message.find("accepted"), std::string::npos);
    std::string plain = "valid UTF-8: \xc3\xa9\n";
    const auto expected = plain;
    el::base::sanitize(plain);
    EXPECT_EQ(plain, expected);
}
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
