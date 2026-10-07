# Wownero FCMP++ source provenance

> Status: draft · Updated 2026-10-07 · Applies to: the `fcmp-testnet` research branch

## References

| Component | Revision |
| --- | --- |
| FCMP++ beta 3.0 base | [`d816367cb1aa`](https://github.com/seraphis-migration/monero/commit/d816367cb1aa405bfa68a20ac3e034d0759d968e) |
| Beta 3.1 legacy ring-offset correction | [`75164cb4f23f`](https://github.com/seraphis-migration/monero/commit/75164cb4f23fabb61d3315dbd397a6962fde4fc7) |
| RandomWOW | [`ff4747966259`](https://github.com/wownero/RandomWOW/commit/ff47479662593fa18295642752b4df8d04c21b66) |

The base retains upstream history and license notices. The research integration
adds Wownero parameters, network isolation, a miner proof of representation and
acceptance tests. Upstream version and seed changes are excluded from the beta
3.1 correction. RandomWOW is pinned to the tested commit containing the public
ARM64 and RISC-V JIT correction.

## Adapted release corrections

| Correction | Upstream commit |
| --- | --- |
| Restricted RPC PoW key-epoch admission | [`837c59bed10d`](https://github.com/monero-project/monero/commit/837c59bed10d06956ca7375a0e91619c0b77142e) |
| Distribution cache preserves bounds on refusal | [`e075ed6baacc`](https://github.com/monero-project/monero/commit/e075ed6baaccac7a8b170a97a48375647600bd5e) |
| Public-node RPC requests public peers only | [`6c5a4c18e3e8`](https://github.com/monero-project/monero/commit/6c5a4c18e3e8cbe088ccaccb0104daf8e4f9afbf) |
| Duplicate transaction hashes rejected before pool lookup | [`03f957c18c88`](https://github.com/monero-project/monero/commit/03f957c18c8818c3c25b75d771fe48c37036b6e3) |
| Console control-character sanitization | [`52b33f4fb01b`](https://github.com/monero-project/monero/commit/52b33f4fb01bd54d936a016cbc63a4850daa3441) |
| Exact pruned transaction boundary at P2P ingress | [`71e3be0e48d0`](https://github.com/monero-project/monero/commit/71e3be0e48d0939dfd2500f703b69234967d254a) |
| Flush spans after preparation failure or peer disconnect | [`dfa2d3020be5`](https://github.com/monero-project/monero/commit/dfa2d3020be5cc6dcd4a525bc1b7a315b9e83f57) |
| Nested LMDB admission across a closed creation gate | [`6ee665b2c228`](https://github.com/monero-project/monero/commit/6ee665b2c228dde4bb576f0ed7edb8d1a1702673) |
| Restricted block templates exclude sensitive transactions | [`1d45021cd0e4`](https://github.com/monero-project/monero/commit/1d45021cd0e452b60846b458701e612ab01db5c7) |

These ports cover selected daemon and common-code changes. They do not establish
complete parity with Monero's release branch. Wallet validation, imported-output
and multisig state, shared networking and dependency recipes require further
review against the FCMP++ implementation.

## Qualification scope

Local macOS ARM64, Linux ARM64 and emulated Linux x86-64 runs cover miner
authorization, network isolation, FCMP payments, restricted templates, restart,
wallet rollback and a spend after reorganization. The focused C++ suite contains
26 cases. Selected RandomWOW interpreter and JIT vectors and the generated FCMP
tables agree across those builds. Emulation does not qualify native deployment
performance.

The branch CI produces fresh native Linux builds and records source and artifact
digests. Its manual network lane verifies the exact successful build before
execution. Build results, network acceptance and deployment are separate evidence.
Neither local nor CI tests replace independent cryptographic review.

## Review checklist

- [ ] Complete release-fix reconciliation with behavioral evidence.
- [ ] Independent cryptographic and side-channel review of miner authorization.
- [ ] Broader reorganization, PoW key-transition and sustained-mining tests.
- [ ] Mainnet migration specification, if proposed separately.
