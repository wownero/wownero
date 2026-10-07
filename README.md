# Wownero FCMP++ testnet

> Status: draft · Updated 2026-10-07 · Applies to: the `fcmp-testnet` research branch

This branch implements an isolated Wownero testnet using FCMP++, Carrot and
RandomWOW. It includes Carrot-compatible miner authorization, Wownero monetary
parameters, a separate network identity and local acceptance tests.

The implementation is unaudited. Mainnet migration, swap primitives and pool
protocols remain outside this branch. Testnet resets require fresh chain data.
Both research build options default to `OFF`; the commands below enable them
explicitly. The resulting daemon and wallets require `--testnet`.

## Build

Linux builds require CMake, a C++17 compiler and Rust 1.93.0. On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libboost-all-dev \
  libssl-dev libzmq3-dev libunbound-dev libsodium-dev libunwind-dev \
  libreadline-dev libhidapi-dev libusb-1.0-0-dev libprotobuf-dev protobuf-compiler
rustup toolchain install 1.93.0 --profile minimal

git clone --branch fcmp-testnet --recurse-submodules https://github.com/wownero/wownero.git
cd wownero
export RUSTUP_TOOLCHAIN=1.93.0
export CARGO_BUILD_JOBS=2
export CARGO_PROFILE_RELEASE_BUILD_OVERRIDE_OPT_LEVEL=3
WOW_BUILD_DIR="$HOME/Build/wownero-fcmp-testnet"
cmake -S . -B "$WOW_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DARCH=default -DBUILD_TESTS=ON -DBUILD_GUI_DEPS=OFF \
  -DWOWNERO_FCMP_TESTNET=ON -DBUILD_WOW_MINER_PROOF_EXPERIMENT=ON
cmake --build "$WOW_BUILD_DIR" --parallel 2 --target \
  daemon simplewallet wallet_rpc_server wow_miner_proof_tests \
  wow_testnet_tests wow_release_regressions wow_pow_vectors wow_local_network
```

The binaries are `wownero-fcmp-testnetd`, `wownero-fcmp-testnet-wallet-cli` and
`wownero-fcmp-testnet-wallet-rpc`, under the build directory's `bin/`.
Use a fresh build directory when changing the source revision or toolchain.

## Verify

```sh
ctest --test-dir "$WOW_BUILD_DIR" --output-on-failure \
  -R '^wow_(miner_proof|testnet|release_regressions)$'
"$WOW_BUILD_DIR/tests/wow_miner_proof/wow_pow_vectors"
"$WOW_BUILD_DIR/tests/wow_miner_proof/wow_local_network" --rpc-read-check
```

The explicit network test performs real proof-of-work mining and FCMP payments:

```sh
WOW_TEST_DATA_DIR="$HOME/Build/wownero-fcmp-network-test"
"$WOW_BUILD_DIR/tests/wow_miner_proof/wow_local_network" \
  "$WOW_BUILD_DIR/bin/wownero-fcmp-testnetd" "$WOW_TEST_DATA_DIR"
```

The data directory must not exist. The harness uses loopback ports
49880–49882 and 49890–49892, creates in-memory wallets, and terminates its
daemons on exit. It verifies maturity, payments, miner authorization, restricted
templates, restart, a two-block wallet rollback and a subsequent FCMP spend.
It retains public chain data and logs, but no recoverable wallet. Mining requires
additional memory beyond the build's idle usage; a two-node run exceeded a 2 GiB
container limit during qualification.

## CI and artifacts

Pushes and pull requests run native Linux x86-64 and ARM64 builds, C++ tests,
PoW checks and the RPC fixture. Artifacts contain the daemon, wallet applications,
network harness, source metadata and SHA-256 checksums. They are research builds.

The manual network lane consumes artifacts from a successful push run at the
same source commit. It refuses a different revision, branch or workflow:

```sh
gh workflow run build.yml --repo wownero/wownero --ref fcmp-testnet \
  -f build_run_id=RUN_ID
```

Replace `RUN_ID` with the successful build run's numeric ID. Network acceptance
and source checks have separate results. Neither constitutes a deployment or
independent cryptographic review.

## References

- [Protocol, build options and test coverage](tests/wow_miner_proof/README.md)
- [Source provenance and qualification scope](docs/wownero-fcmp-provenance.md)
- [FCMP++ reference](https://github.com/seraphis-migration/monero)
- [RandomWOW](https://github.com/wownero/RandomWOW)

## Review checklist

- [ ] Independent review of miner authorization and its block binding.
- [ ] Complete release-fix reconciliation, including wallet and networking paths.
- [ ] Deeper reorganizations, PoW key transitions and sustained mining measurements.
- [ ] Deployment-specific artifact, resource and network qualification.
