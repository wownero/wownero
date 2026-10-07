// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include "wow_miner.h"

#include <chrono>
#include <cstring>
#include <cerrno>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include "carrot_core/account_secrets.h"
#include "carrot_core/device_ram_borrowed.h"
#include "carrot_core/scan.h"
#include "cryptonote_format_utils.h"
#include "string_tools.h"
#include "wownero/testnet_genesis.h"

namespace cryptonote
{
namespace
{
template<class T> unsigned char *bytes(T &value)
{
    return reinterpret_cast<unsigned char *>(value.data);
}
template<class T> const unsigned char *bytes(const T &value)
{
    return reinterpret_cast<const unsigned char *>(value.data);
}

const crypto::hash &network_genesis()
{
    static const crypto::hash value = [] {
        crypto::hash result;
        if (!epee::string_tools::hex_to_pod(wownero::testnet::genesis_hash, result))
            throw std::logic_error("Invalid compiled Wownero testnet genesis hash");
        return result;
    }();
    return value;
}

const txout_to_carrot_v1 *miner_output(const block &b)
{
    if (b.major_version < HF_VERSION_CARROT || b.wow_vote > 2 ||
        b.miner_tx.vin.size() != 1 || b.miner_tx.vout.size() != 1 ||
        !boost::strict_get<txin_gen>(&b.miner_tx.vin.front()))
        return nullptr;
    return boost::strict_get<txout_to_carrot_v1>(&b.miner_tx.vout.front().target);
}
}

bool wow_miner_account::address(account_public_address &out) const
{
    if (sc_check(bytes(generate_image)) || sc_check(bytes(prove_spend)) ||
        sc_check(bytes(view_incoming)) || !sc_isnonzero(bytes(view_incoming)))
        return false;
    carrot::make_carrot_spend_pubkey(generate_image, prove_spend, out.m_spend_public_key);
    return wownero::research::check_miner_public_key(out.m_spend_public_key) &&
        crypto::secret_key_to_public_key(view_incoming, out.m_view_public_key);
}

bool read_wow_miner_account(int fd, wow_miner_account &out)
{
    out = {};
    if (fd < 3)
        return false;
    struct descriptor_guard { int fd; ~descriptor_guard() { close(fd); } } guard{fd};
    struct stat info{};
    if (fstat(fd, &info) != 0 || !S_ISFIFO(info.st_mode))
        return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    auto read_chunk = [&](void *destination, std::size_t size) {
        auto *cursor = static_cast<unsigned char *>(destination);
        while (size)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0)
                return false;
            pollfd ready{fd, POLLIN, 0};
            const int status = poll(&ready, 1, static_cast<int>(remaining));
            if (status < 0 && errno == EINTR)
                continue;
            if (status <= 0 || (ready.revents & (POLLERR | POLLNVAL)))
                return false;
            const ssize_t count = read(fd, cursor, size);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                return false;
            cursor += count;
            size -= count;
        }
        return true;
    };
    char magic[8];
    wow_miner_account candidate;
    if (!read_chunk(magic, sizeof(magic)) || std::memcmp(magic, "WOWMIN01", 8) ||
        !read_chunk(candidate.generate_image.data, 32) ||
        !read_chunk(candidate.prove_spend.data, 32) ||
        !read_chunk(candidate.view_incoming.data, 32))
        return false;
    // The fixed frame is sufficient. The input descriptor is closed immediately.
    account_public_address address;
    if (!candidate.address(address))
        return false;
    out = candidate;
    return true;
}

crypto::hash get_wow_miner_statement_hash(const block &b)
{
    block statement = b;
    statement.wow_miner_signature.fill(0);
    statement.invalidate_hashes();
    statement.miner_tx.invalidate_hashes();
    const auto blob = get_block_hashing_blob(statement);
    return crypto::cn_fast_hash(blob.data(), blob.size());
}

bool prepare_wow_miner_output_keys(const block &b, const wow_miner_account &account,
    crypto::secret_key &x, crypto::secret_key &y)
{
    x = {}; y = {};
    const auto *output = miner_output(b);
    account_public_address address;
    if (!output || !account.address(address))
        return false;
    std::vector<tx_extra_field> fields;
    if (!parse_tx_extra(b.miner_tx.extra, fields))
        return false;
    const tx_extra_pub_key *ephemeral = nullptr;
    for (const auto &field : fields)
    {
        if (const auto *key = boost::strict_get<tx_extra_pub_key>(&field))
        {
            if (ephemeral)
                return false;
            ephemeral = key;
        }
        if (boost::strict_get<tx_extra_additional_pub_keys>(&field))
            return false;
    }
    if (!ephemeral)
        return false;
    carrot::CarrotCoinbaseEnoteV1 enote;
    enote.block_index = boost::get<txin_gen>(b.miner_tx.vin.front()).height;
    enote.amount = b.miner_tx.vout.front().amount;
    enote.onetime_address = output->key;
    enote.view_tag = output->view_tag;
    enote.anchor_enc = output->encrypted_janus_anchor;
    static_assert(sizeof(enote.enote_ephemeral_pubkey) == sizeof(ephemeral->pub_key));
    std::memcpy(&enote.enote_ephemeral_pubkey, &ephemeral->pub_key, sizeof(ephemeral->pub_key));
    carrot::view_incoming_key_ram_borrowed_device device(account.view_incoming);
    mx25519_pubkey shared;
    crypto::secret_key extension_g, extension_t;
    if (!carrot::try_make_carrot_shared_key_receiver(device, enote.enote_ephemeral_pubkey, shared) ||
        !carrot::try_scan_carrot_coinbase_enote_receiver(enote, shared, address.m_spend_public_key,
            address.m_view_public_key, extension_g, extension_t))
        return false;
    sc_add(bytes(x), bytes(account.generate_image), bytes(extension_g));
    sc_add(bytes(y), bytes(account.prove_spend), bytes(extension_t));
    return true;
}

bool sign_wow_miner_block(block &b, const crypto::secret_key &x, const crypto::secret_key &y)
{
    b.wow_miner_signature.fill(0);
    b.invalidate_hashes();
    const auto *output = miner_output(b);
    return output && wownero::research::generate_miner_proof(network_genesis(),
        get_wow_miner_statement_hash(b), output->key, x, y, b.wow_miner_signature);
}

bool verify_wow_miner_block(const block &b)
{
    const auto *output = miner_output(b);
    return output && wownero::research::verify_miner_proof(network_genesis(),
        get_wow_miner_statement_hash(b), output->key,
        {b.wow_miner_signature.data(), b.wow_miner_signature.size()});
}
}
