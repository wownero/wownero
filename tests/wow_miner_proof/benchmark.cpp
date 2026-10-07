// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include "fixture.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

namespace wr = wownero::research;

template<class F> double measure(F &&operation)
{
    const auto start = std::chrono::steady_clock::now();
    if (!operation())
        throw std::runtime_error("Benchmark operation failed validation");
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
}

void report(const char *name, std::vector<double> &samples)
{
    std::sort(samples.begin(), samples.end());
    std::cout << "  \"" << name << "\": {\"median_us\": " << samples[samples.size() / 2]
        << ", \"p95_us\": " << samples[(samples.size() * 95 + 99) / 100 - 1] << "}";
}

int main()
{
    try
    {
        constexpr std::size_t count = 1000;
        const wr::test::coinbase_fixture fixture;
        crypto::secret_key legacy_secret;
        crypto::public_key legacy_public;
        crypto::generate_keys(legacy_public, legacy_secret);
        std::vector<double> prove, verify, legacy_prove, legacy_verify;
        for (std::size_t i = 0; i < count + 10; ++i)
        {
            wr::miner_proof proof;
            const auto p = measure([&] { return wr::generate_miner_proof(fixture.network,
                fixture.message, fixture.enote.onetime_address, fixture.x, fixture.y, proof); });
            const auto v = measure([&] { return fixture.verify(proof); });
            crypto::signature signature;
            const auto lp = measure([&] {
                crypto::generate_signature(fixture.message, legacy_public, legacy_secret, signature);
                return true;
            });
            const auto lv = measure([&] {
                return crypto::check_signature(fixture.message, legacy_public, signature);
            });
            if (i >= 10)
            {
                prove.push_back(p);
                verify.push_back(v);
                legacy_prove.push_back(lp);
                legacy_verify.push_back(lv);
            }
        }
        std::cout << std::fixed << std::setprecision(3)
            << "{\n  \"samples\": " << count << ",\n  \"warmups\": 10,\n"
            << "  \"proof_bytes\": " << wr::miner_proof{}.size() << ",\n"
            << "  \"scope\": \"native primitive; fixed statement; fresh nonces; no block validation or PoW\",\n";
        report("prove", prove);
        std::cout << ",\n";
        report("verify", verify);
        std::cout << ",\n";
        report("legacy_prove", legacy_prove);
        std::cout << ",\n";
        report("legacy_verify", legacy_verify);
        std::cout << "\n}\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
