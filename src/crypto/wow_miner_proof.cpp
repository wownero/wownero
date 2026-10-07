// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include "wow_miner_proof.h"

#include <cstring>

#include "generators.h"

namespace wownero::research
{
namespace
{
constexpr char domain[] = "WOW-CARROT-MINER-PROOF-v1";
constexpr unsigned char subgroup_order[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10};

template<class T> const unsigned char *bytes(const T &value)
{
    return reinterpret_cast<const unsigned char *>(value.data);
}

template<class T> unsigned char *bytes(T &value)
{
    return reinterpret_cast<unsigned char *>(value.data);
}

bool decode_public_key(const crypto::public_key &key, ge_p3 &point)
{
    if (ge_frombytes_vartime(&point, bytes(key)) != 0 ||
        ge_p3_is_point_at_infinity_vartime(&point))
        return false;

    crypto::public_key canonical;
    ge_p3_tobytes(bytes(canonical), &point);
    if (std::memcmp(key.data, canonical.data, sizeof(key.data)) != 0)
        return false;

    // Multiplication uses the unreduced subgroup order to reject mixed-order points.
    ge_p3 multiplied;
    ge_scalarmult_p3(&multiplied, subgroup_order, &point);
    return ge_p3_is_point_at_infinity_vartime(&multiplied);
}

void representation(const unsigned char *x, const unsigned char *y, ge_p3 &out)
{
    ge_p3 xg, yt;
    ge_scalarmult_base(&xg, x);
    ge_scalarmult_p3(&yt, y, &crypto::get_T_p3());
    ge_cached cached;
    ge_p3_to_cached(&cached, &yt);
    ge_p1p1 sum;
    ge_add(&sum, &xg, &cached);
    ge_p1p1_to_p3(&out, &sum);
}

crypto::ec_scalar challenge(const crypto::hash &network_genesis,
    const crypto::hash &block_digest, const crypto::public_key &key, const ge_p3 &nonce)
{
    std::array<unsigned char, sizeof(domain) - 1 + 4 * 32> transcript{};
    auto *cursor = transcript.data();
    std::memcpy(cursor, domain, sizeof(domain) - 1);
    cursor += sizeof(domain) - 1;
    for (const auto *field : {bytes(network_genesis), bytes(block_digest), bytes(key)})
    {
        std::memcpy(cursor, field, 32);
        cursor += 32;
    }
    ge_p3_tobytes(cursor, &nonce);
    crypto::ec_scalar result;
    crypto::hash_to_scalar(transcript.data(), transcript.size(), result);
    return result;
}
}

bool check_miner_public_key(const crypto::public_key &key)
{
    ge_p3 point;
    return decode_public_key(key, point);
}

bool generate_miner_proof(const crypto::hash &network_genesis,
    const crypto::hash &block_digest, const crypto::public_key &key,
    const crypto::secret_key &x, const crypto::secret_key &y, miner_proof &proof)
{
    proof.fill(0);
    if (sc_check(bytes(x)) != 0 || sc_check(bytes(y)) != 0 || !check_miner_public_key(key))
        return false;

    ge_p3 expected;
    representation(bytes(x), bytes(y), expected);
    crypto::public_key encoded;
    ge_p3_tobytes(bytes(encoded), &expected);
    if (std::memcmp(key.data, encoded.data, sizeof(key.data)) != 0)
        return false;

    // Secret scalars use the upstream locked and scrubbed key container.
    crypto::secret_key a, b;
    for (;;)
    {
        crypto::random32_unbiased(bytes(a));
        crypto::random32_unbiased(bytes(b));
        ge_p3 nonce;
        representation(bytes(a), bytes(b), nonce);
        if (ge_p3_is_point_at_infinity_vartime(&nonce))
            continue;

        const crypto::ec_scalar c = challenge(network_genesis, block_digest, key, nonce);
        if (!sc_isnonzero(bytes(c)))
            continue;
        std::memcpy(proof.data(), c.data, 32);
        sc_muladd(proof.data() + 32, bytes(c), bytes(x), bytes(a));
        sc_muladd(proof.data() + 64, bytes(c), bytes(y), bytes(b));
        return true;
    }
}

bool verify_miner_proof(const crypto::hash &network_genesis,
    const crypto::hash &block_digest, const crypto::public_key &key,
    epee::span<const std::uint8_t> proof)
{
    if (proof.size() != miner_proof{}.size())
        return false;
    const auto *c = proof.data();
    const auto *sg = c + 32;
    const auto *st = sg + 32;
    if (sc_check(c) != 0 || sc_check(sg) != 0 || sc_check(st) != 0 || !sc_isnonzero(c))
        return false;

    ge_p3 public_point;
    if (!decode_public_key(key, public_point))
        return false;

    ge_p3 response, ck;
    representation(sg, st, response);
    ge_scalarmult_p3(&ck, c, &public_point);
    ge_cached cached;
    ge_p3_to_cached(&cached, &ck);
    ge_p1p1 difference;
    ge_sub(&difference, &response, &cached);
    ge_p3 nonce;
    ge_p1p1_to_p3(&nonce, &difference);
    if (ge_p3_is_point_at_infinity_vartime(&nonce))
        return false;

    const auto expected = challenge(network_genesis, block_digest, key, nonce);
    return std::memcmp(expected.data, c, 32) == 0;
}
}
