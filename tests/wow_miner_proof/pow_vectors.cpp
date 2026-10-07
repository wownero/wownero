// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "randomx.h"
#include "string_tools.h"

int main()
{
    try
    {
        const std::string seed = "Wownero FCMP testnet PoW qualification v1";
        const std::vector<std::string> inputs{
            "Wownero FCMP testnet genesis domain",
            "Wownero FCMP testnet fork A",
            "Wownero FCMP testnet fork B",
            std::string(256, '\0')};
        std::vector<std::array<char, RANDOMX_HASH_SIZE>> expected;
        for (const auto flags : {RANDOMX_FLAG_DEFAULT, RANDOMX_FLAG_JIT})
        {
            std::unique_ptr<randomx_cache, decltype(&randomx_release_cache)> cache(
                randomx_alloc_cache(flags), randomx_release_cache);
            if (!cache)
                throw std::runtime_error("Unable to allocate the RandomWOW qualification cache");
            randomx_init_cache(cache.get(), seed.data(), seed.size());
            std::unique_ptr<randomx_vm, decltype(&randomx_destroy_vm)> vm(
                randomx_create_vm(flags | RANDOMX_FLAG_SECURE, cache.get(), nullptr), randomx_destroy_vm);
            if (!vm)
                throw std::runtime_error("Unable to create the requested RandomWOW VM");
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                std::array<char, RANDOMX_HASH_SIZE> hash{};
                randomx_calculate_hash(vm.get(), inputs[i].data(), inputs[i].size(), hash.data());
                if (flags == RANDOMX_FLAG_DEFAULT)
                    expected.push_back(hash);
                else if (hash != expected.at(i))
                    throw std::runtime_error("RandomWOW interpreter and JIT disagree");
            }
        }
        for (size_t i = 0; i < expected.size(); ++i)
            std::cout << "vector=" << i << " hash="
                << epee::string_tools::buff_to_hex_nodelimer(std::string(expected[i].data(), expected[i].size()))
                << '\n';
        std::cout << "interpreter_jit_agreement=true\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "PoW qualification failed: " << error.what() << '\n';
        return 1;
    }
}
