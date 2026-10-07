# Copyright (c) 2026, The Wownero Project
# SPDX-License-Identifier: BSD-3-Clause

if(WOWNERO_FCMP_TESTNET)
  add_library(wow_miner_proof_experiment INTERFACE)
  target_link_libraries(wow_miner_proof_experiment INTERFACE cncrypto)
else()
  monero_add_library(wow_miner_proof_experiment
    ../../src/crypto/wow_miner_proof.cpp)
  target_link_libraries(wow_miner_proof_experiment PUBLIC cncrypto)
endif()

monero_add_minimal_executable(wow_miner_proof_tests tests.cpp)
target_link_libraries(wow_miner_proof_tests PRIVATE
  wow_miner_proof_experiment carrot_core ${GTEST_LIBRARIES})
add_test(NAME wow_miner_proof COMMAND wow_miner_proof_tests)

monero_add_minimal_executable(wow_miner_proof_benchmark benchmark.cpp)
target_link_libraries(wow_miner_proof_benchmark PRIVATE
  wow_miner_proof_experiment carrot_core)

monero_add_minimal_executable(wow_testnet_genesis genesis.cpp)
target_link_libraries(wow_testnet_genesis PRIVATE cryptonote_basic)

if(WOWNERO_FCMP_TESTNET)
  monero_add_minimal_executable(wow_testnet_tests testnet.cpp)
  target_link_libraries(wow_testnet_tests PRIVATE cryptonote_basic ${GTEST_LIBRARIES})
  add_test(NAME wow_testnet COMMAND wow_testnet_tests)
  monero_add_minimal_executable(wow_release_regressions release_regressions.cpp)
  target_link_libraries(wow_release_regressions PRIVATE
    cryptonote_core rpc_base ${GTEST_LIBRARIES})
  add_test(NAME wow_release_regressions COMMAND wow_release_regressions)
  monero_add_minimal_executable(wow_pow_vectors pow_vectors.cpp)
  target_include_directories(wow_pow_vectors PRIVATE ../../external/randomwow/src)
  target_link_libraries(wow_pow_vectors PRIVATE randomx epee)
  monero_add_minimal_executable(wow_local_network local_network.cpp)
  target_link_libraries(wow_local_network PRIVATE wallet cryptonote_basic)
endif()
