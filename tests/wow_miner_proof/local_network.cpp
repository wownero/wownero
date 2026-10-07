// Copyright (c) 2026, The Wownero Project
// SPDX-License-Identifier: BSD-3-Clause

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <boost/asio.hpp>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cryptonote_basic/account.h"
#include "cryptonote_basic/wow_miner.h"
#include "cryptonote_basic/difficulty.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "rpc/core_rpc_server_error_codes.h"
#include "net/http_client.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "wallet/wallet2.h"

namespace
{
using namespace cryptonote;
using namespace std::chrono_literals;

void require(bool condition, const char *reason)
{
    if (!condition)
        throw std::runtime_error(reason);
}

struct local_node
{
    pid_t pid = -1;
    epee::net_utils::http::http_simple_client rpc;

    ~local_node() { stop(); }
    void stop()
    {
        if (pid < 0)
            return;
        kill(pid, SIGTERM);
        for (unsigned i = 0; i < 300; ++i)
        {
            if (waitpid(pid, nullptr, WNOHANG) == pid)
            {
                pid = -1;
                return;
            }
            std::this_thread::sleep_for(100ms);
        }
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        pid = -1;
    }

    void start(const std::string &binary, const std::filesystem::path &data, unsigned port,
        unsigned peer, const wow_miner_account *account = nullptr, bool offline = false)
    {
        std::filesystem::create_directories(data);
        int keys[2] = {-1, -1};
        if (account)
        {
            require(pipe(keys) == 0, "Unable to create the mining credential pipe");
            require(write(keys[1], "WOWMIN01", 8) == 8, "Unable to write mining frame header");
            for (const auto *key : {&account->generate_image, &account->prove_spend, &account->view_incoming})
                require(write(keys[1], key->data, 32) == 32, "Unable to write mining frame");
            close(keys[1]);
        }
        const int log = open((data / "console.log").c_str(), O_CREAT | O_WRONLY | O_APPEND, 0600);
        require(log >= 0, "Unable to open the local daemon log");
        std::vector<std::string> arguments{binary, "--testnet", "--non-interactive", "--no-zmq",
            "--data-dir", data.string(), "--log-file", (data / "daemon.log").string(),
            "--p2p-bind-ip", "127.0.0.1", "--p2p-bind-port", std::to_string(port),
            "--rpc-bind-ip", "127.0.0.1", "--rpc-bind-port", std::to_string(port + 1),
            "--rpc-restricted-bind-port", std::to_string(port + 2),
            "--rpc-ssl", "disabled",
            "--allow-local-ip", "--hide-my-port", "--no-igd", "--max-concurrency", "2",
            "--check-updates", "disabled", "--disable-dns-checkpoints", "--log-level", "0"};
        if (offline)
            arguments.emplace_back("--offline");
        if (peer)
        {
            arguments.emplace_back("--add-exclusive-node");
            arguments.push_back("127.0.0.1:" + std::to_string(peer));
        }
        if (account)
        {
            arguments.emplace_back("--wow-miner-key-fd");
            arguments.push_back(std::to_string(keys[0]));
        }
        std::vector<char *> argv;
        for (auto &argument : arguments)
            argv.push_back(argument.data());
        argv.push_back(nullptr);
        pid = fork();
        require(pid >= 0, "Unable to fork the local daemon");
        if (pid == 0)
        {
            dup2(log, STDOUT_FILENO);
            dup2(log, STDERR_FILENO);
            close(log);
            execv(binary.c_str(), argv.data());
            _exit(127);
        }
        close(log);
        if (keys[0] >= 0)
            close(keys[0]);
        require(rpc.set_server("127.0.0.1:" + std::to_string(port + 1), boost::none), "Invalid local RPC endpoint");
        const auto deadline = std::chrono::steady_clock::now() + 90s;
        while (std::chrono::steady_clock::now() < deadline)
        {
            int status;
            if (waitpid(pid, &status, WNOHANG) == pid)
            {
                pid = -1;
                throw std::runtime_error("Local daemon exited before RPC startup; inspect its local log");
            }
            COMMAND_RPC_GET_INFO::response info{};
            if (get_info(info))
            {
                require(!std::filesystem::exists(data / "testnet" / "rpc_ssl.key"),
                    "Loopback daemon created an RPC private-key file");
                return;
            }
            std::this_thread::sleep_for(500ms);
        }
        throw std::runtime_error("Local daemon RPC startup timed out");
    }

    bool get_info(COMMAND_RPC_GET_INFO::response &info)
    {
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        {
            info = {};
            if (epee::net_utils::invoke_http_json("/get_info", COMMAND_RPC_GET_INFO::request{}, info, rpc, 30s))
            {
                if (info.status == CORE_RPC_STATUS_OK)
                    return true;
                std::cerr << "get_info refused: " << info.status << std::endl;
                return false;
            }
            if (attempt == 0)
            {
                std::cerr << "get_info transport or decoding failed; reconnecting once" << std::endl;
                rpc.disconnect();
            }
        }
        std::cerr << "get_info transport or decoding failed after reconnect" << std::endl;
        return false;
    }
    void mining(const std::string &address, bool enable)
    {
        if (enable)
        {
            COMMAND_RPC_START_MINING::request req{};
            COMMAND_RPC_START_MINING::response res{};
            req.miner_address = address;
            req.threads_count = 2;
            req.ignore_battery = true;
            const auto deadline = std::chrono::steady_clock::now() + 90s;
            do
            {
                res = {};
                require(epee::net_utils::invoke_http_json("/start_mining", req, res, rpc, 30s),
                    "Local mining start RPC transport failed");
                if (res.status == CORE_RPC_STATUS_OK)
                    return;
                if (res.status != CORE_RPC_STATUS_BUSY)
                    throw std::runtime_error("Local mining start refused: " + res.status);
                std::this_thread::sleep_for(250ms);
            } while (std::chrono::steady_clock::now() < deadline);
            throw std::runtime_error("Local mining start remained busy after the readiness deadline");
        }
        else
        {
            COMMAND_RPC_STOP_MINING::response res{};
            require(epee::net_utils::invoke_http_json("/stop_mining", COMMAND_RPC_STOP_MINING::request{}, res, rpc, 30s)
                && res.status == CORE_RPC_STATUS_OK, "Local mining stop failed");
        }
    }
};


void verify_read_reconnect()
{
    using tcp = boost::asio::ip::tcp;
    boost::asio::io_context io;
    tcp::acceptor listener(io, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
    std::atomic<bool> first_request_dropped{false};
    std::shared_ptr<tcp::socket> responding_socket;
    COMMAND_RPC_GET_INFO::response expected{};
    expected.status = CORE_RPC_STATUS_OK;
    expected.height = 42;
    const std::string body = epee::serialization::store_t_to_json(expected);
    const auto reply = std::make_shared<std::string>(
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    std::function<void()> accept;
    accept = [&] {
        const auto socket = std::make_shared<tcp::socket>(io);
        listener.async_accept(*socket, [&, socket](const boost::system::error_code &error) {
            if (error)
                return;
            const auto request = std::make_shared<boost::asio::streambuf>();
            boost::asio::async_read_until(*socket, *request, "\r\n\r\n",
                [&, socket, request](const boost::system::error_code &read_error, size_t) {
                    if (read_error)
                        return;
                    if (!first_request_dropped.exchange(true))
                    {
                        boost::system::error_code ignored;
                        socket->close(ignored);
                        accept();
                        return;
                    }
                    responding_socket = socket;
                    // Transaction validation can hold the daemon lock beyond a short read deadline.
                    const auto delay = std::make_shared<boost::asio::steady_timer>(io, 6s);
                    delay->async_wait([socket, reply, delay](const boost::system::error_code &wait_error) {
                        if (wait_error)
                            return;
                        boost::asio::async_write(*socket, boost::asio::buffer(*reply),
                            [socket, reply](const boost::system::error_code &, size_t) {
                                boost::system::error_code ignored;
                                socket->shutdown(tcp::socket::shutdown_send, ignored);
                            });
                    });
                });
        });
    };
    local_node client;
    require(client.rpc.set_server("127.0.0.1:" + std::to_string(listener.local_endpoint().port()),
        boost::none, epee::net_utils::ssl_support_t::e_ssl_support_disabled),
        "Unable to configure the read-reconnection fixture");
    accept();
    std::thread server([&] { io.run(); });
    COMMAND_RPC_GET_INFO::response observed{};
    bool received = false;
    try
    {
        received = client.get_info(observed);
    }
    catch (...)
    {
        io.stop();
        server.join();
        throw;
    }
    io.stop();
    server.join();
    require(first_request_dropped && received && observed.height == expected.height,
        "Read-only RPC did not recover the response after a dropped connection");
    std::cout << "read_rpc_reconnect_verified=true" << std::endl;
}

struct prepared_block
{
    block candidate;
    crypto::hash seed;
    crypto::secret_key x, y;
    uint64_t difficulty;

    prepared_block(local_node &node, const std::string &address, const wow_miner_account &account)
    {
        COMMAND_RPC_GETBLOCKTEMPLATE::request req{};
        COMMAND_RPC_GETBLOCKTEMPLATE::response res{};
        req.wallet_address = address;
        require(epee::net_utils::invoke_http_json_rpc("/json_rpc", "get_block_template", req, res, node.rpc)
            && res.status == CORE_RPC_STATUS_OK, "Unable to obtain a test block template");
        require(res.difficulty_top64 == 0, "Unexpected local test difficulty width");
        std::string blob;
        require(epee::string_tools::parse_hexstr_to_binbuff(res.blocktemplate_blob, blob)
            && parse_and_validate_block_from_blob(blob, candidate)
            && epee::string_tools::hex_to_pod(res.seed_hash, seed), "Invalid local block template");
        require(prepare_wow_miner_output_keys(candidate, account, x, y), "Unable to recover template witnesses");
        difficulty = res.difficulty;
    }

    void solve(bool authorize)
    {
        const auto deadline = std::chrono::steady_clock::now() + 90s;
        do
        {
            candidate.invalidate_hashes();
            if (authorize)
                require(sign_wow_miner_block(candidate, x, y), "Unable to authorize the test block");
            else
                candidate.wow_miner_signature.fill(0);
            crypto::hash pow;
            get_altblock_longhash(candidate, pow, seed);
            if (check_hash(pow, difficulty))
                return;
            ++candidate.nonce;
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("Test block proof-of-work search timed out");
    }
};

void submit_block(local_node &node, const block &candidate, bool expect_acceptance)
{
    COMMAND_RPC_SUBMITBLOCK::request req{epee::string_tools::buff_to_hex_nodelimer(block_to_blob(candidate))};
    COMMAND_RPC_SUBMITBLOCK::response res{};
    epee::json_rpc::error error{};
    const bool accepted = epee::net_utils::invoke_http_json_rpc("/json_rpc", "submit_block",
        req, res, error, node.rpc, 30s);
    if (expect_acceptance)
        require(accepted && res.status == CORE_RPC_STATUS_OK, "Authorized test block was not accepted");
    else
        require(!accepted && error.code == CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED,
            "Invalid test block was not explicitly rejected");
}

void verify_miner_proof_enforcement(local_node &node, const std::string &address,
    const wow_miner_account &account)
{
    COMMAND_RPC_GET_INFO::response before{}, after{};
    require(node.get_info(before), "Unable to read the chain before authorization tests");
    prepared_block test(node, address, account);
    test.solve(false);
    submit_block(node, test.candidate, false);
    require(node.get_info(after) && after.top_block_hash == before.top_block_hash,
        "Unauthorized block changed the chain tip");
    test.solve(true);
    submit_block(node, test.candidate, true);
    std::cout << "unauthorized_pow_block_rejected=true authorized_control_accepted=true" << std::endl;
}

void verify_restricted_templates(local_node &node, const std::string &address,
    const std::vector<tools::wallet2::pending_tx> &transfers)
{
    COMMAND_RPC_FLUSH_TRANSACTION_POOL::request flush{};
    for (const auto &transfer : transfers)
    {
        COMMAND_RPC_SEND_RAW_TX::request req{};
        COMMAND_RPC_SEND_RAW_TX::response res{};
        req.tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(tx_to_blob(transfer.tx));
        req.do_not_relay = true;
        req.do_sanity_checks = true;
        require(epee::net_utils::invoke_http_json("/send_raw_transaction", req, res, node.rpc, 30s)
            && res.status == CORE_RPC_STATUS_OK && res.not_relayed, "Unable to admit a private test transaction");
        flush.txids.push_back(epee::string_tools::pod_to_hex(get_transaction_hash(transfer.tx)));
    }
    epee::net_utils::http::http_simple_client restricted;
    require(restricted.set_server("127.0.0.1:49882", boost::none), "Invalid restricted test endpoint");
    // Alternate callers to exercise both directions of the shared template cache.
    for (const bool public_rpc : {false, true, true, false, true})
    {
        COMMAND_RPC_GETBLOCKTEMPLATE::request req{};
        COMMAND_RPC_GETBLOCKTEMPLATE::response res{};
        req.wallet_address = address;
        auto &client = public_rpc ? restricted : node.rpc;
        require(epee::net_utils::invoke_http_json_rpc("/json_rpc", "get_block_template", req, res, client)
            && res.status == CORE_RPC_STATUS_OK, "Template policy test RPC failed");
        std::string blob;
        block b;
        require(epee::string_tools::parse_hexstr_to_binbuff(res.blocktemplate_blob, blob)
            && parse_and_validate_block_from_blob(blob, b), "Template policy test returned an invalid block");
        for (const auto &transfer : transfers)
        {
            const bool included = std::find(b.tx_hashes.begin(), b.tx_hashes.end(),
                get_transaction_hash(transfer.tx)) != b.tx_hashes.end();
            require(included == !public_rpc, "Template disclosed a private transaction or omitted the private control");
        }
    }
    COMMAND_RPC_FLUSH_TRANSACTION_POOL::response flushed{};
    require(epee::net_utils::invoke_http_json_rpc("/json_rpc", "flush_txpool", flush, flushed, node.rpc)
        && flushed.status == CORE_RPC_STATUS_OK, "Unable to clear the private test transactions");
    std::cout << "restricted_template_privacy_verified=true" << std::endl;
}

void verify_reorganization(local_node &miner, local_node &observer, const std::string &binary,
    const std::filesystem::path &data, const std::string &address, const wow_miner_account &account,
    tools::wallet2 &recovering_wallet)
{
    COMMAND_RPC_GET_INFO::response common{}, competing{}, tip{};
    require(miner.get_info(common) && observer.get_info(tip) && common.top_block_hash == tip.top_block_hash,
        "Reorganization test requires a common starting tip");
    miner.stop();
    observer.stop();
    miner.start(binary, data / "miner", 49880, 0, &account, true);
    observer.start(binary, data / "observer", 49890, 0, &account, true);
    for (unsigned i = 0; i < 2; ++i)
    {
        prepared_block b(miner, address, account);
        b.solve(true);
        submit_block(miner, b.candidate, true);
    }
    require(miner.get_info(competing) && competing.height == common.height + 2,
        "Short fork did not advance by two blocks");
    require(recovering_wallet.init("http://127.0.0.1:49881"), "Unable to select the losing fork for wallet scanning");
    recovering_wallet.refresh(true);
    require(recovering_wallet.get_blockchain_current_height() == competing.height,
        "The recovering wallet did not scan the losing fork");

    prepared_block alternative(observer, address, account);
    alternative.solve(false);
    submit_block(miner, alternative.candidate, false);
    require(miner.get_info(tip) && tip.top_block_hash == competing.top_block_hash,
        "Unauthorized alternate block changed the chain tip");

    std::vector<block> fork;
    alternative.solve(true);
    submit_block(observer, alternative.candidate, true);
    fork.push_back(alternative.candidate);
    for (unsigned i = 0; i < 3; ++i)
    {
        prepared_block b(observer, address, account);
        b.solve(true);
        submit_block(observer, b.candidate, true);
        fork.push_back(b.candidate);
    }
    submit_block(miner, fork.front(), true);
    require(miner.get_info(tip) && tip.top_block_hash == competing.top_block_hash,
        "A shorter valid alternate chain displaced the main chain");
    for (size_t i = 1; i < fork.size(); ++i)
        submit_block(miner, fork[i], true);
    require(observer.get_info(competing) && miner.get_info(tip)
        && tip.top_block_hash == competing.top_block_hash && tip.height == common.height + fork.size(),
        "The node did not adopt the greater-work authorized fork");
    miner.stop();
    observer.stop();
    miner.start(binary, data / "miner", 49880, 49890, &account);
    observer.start(binary, data / "observer", 49890, 49880);
    require(miner.get_info(tip) && tip.top_block_hash == competing.top_block_hash
        && observer.get_info(tip) && tip.top_block_hash == competing.top_block_hash,
        "Restart did not preserve the reorganized chain");
    require(recovering_wallet.init("http://127.0.0.1:49891"), "Unable to restore the observer wallet endpoint");
    recovering_wallet.refresh(true);
    require(recovering_wallet.get_blockchain_current_height() == competing.height,
        "The recovering wallet did not adopt the winning fork");
    std::cout << "alternate_unauthorized_pow_rejected=true reorganization_verified=true wallet_reorganization_verified=true detached_blocks=2" << std::endl;
}

void wait_height(local_node &node, uint64_t wanted)
{
    const auto deadline = std::chrono::steady_clock::now() + 15min;
    auto next_report = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline)
    {
        COMMAND_RPC_GET_INFO::response info{};
        require(node.get_info(info), "Local get_info did not return a valid response");
        if (info.height >= wanted)
            return;
        if (std::chrono::steady_clock::now() >= next_report)
        {
            std::cout << "height=" << info.height << " target=" << wanted << std::endl;
            next_report = std::chrono::steady_clock::now() + 20s;
        }
        std::this_thread::sleep_for(250ms);
    }
    throw std::runtime_error("Local mining or synchronization timed out");
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--rpc-read-check")
        {
            verify_read_reconnect();
            return 0;
        }
        require(argc == 3, "Usage: wow_local_network DAEMON NEW_DATA_DIRECTORY | --rpc-read-check");
        verify_read_reconnect();
        const std::filesystem::path data = std::filesystem::absolute(argv[2]);
        require(!std::filesystem::exists(data), "The local test requires a new data directory");
        account_base source, destination;
        source.generate();
        destination.generate();
        const auto &keys = source.get_keys();
        wow_miner_account miner_account;
        miner_account.generate_image = keys.m_spend_secret_key;
        miner_account.view_incoming = keys.m_view_secret_key;
        const auto address = source.get_public_address_str(TESTNET);
        local_node miner, observer;
        miner.start(argv[1], data / "miner", 49880, 49890, &miner_account);
        observer.start(argv[1], data / "observer", 49890, 49880);
        miner.mining(address, true);
        wait_height(miner, CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW + 12);
        miner.mining(address, false);
        COMMAND_RPC_GET_INFO::response mined{};
        require(miner.get_info(mined), "Miner RPC failed after stopping");
        wait_height(observer, mined.height);
        COMMAND_RPC_GET_INFO::response observed{};
        require(observer.get_info(observed) && observed.top_block_hash == mined.top_block_hash,
            "The local nodes disagree on the mined chain");
        std::cout << "synchronized_height=" << mined.height << std::endl;

        tools::wallet2 sender(TESTNET, 1, true), receiver(TESTNET, 1, true);
        sender.ask_password(tools::wallet2::AskPasswordNever);
        receiver.ask_password(tools::wallet2::AskPasswordNever);
        sender.generate("", "", keys.m_account_address, keys.m_spend_secret_key, keys.m_view_secret_key);
        const auto &recipient_keys = destination.get_keys();
        receiver.generate("", "", recipient_keys.m_account_address,
            recipient_keys.m_spend_secret_key, recipient_keys.m_view_secret_key);
        require(sender.init("http://127.0.0.1:49881") && receiver.init("http://127.0.0.1:49891"),
            "Unable to initialize the in-memory test wallets");
        sender.refresh(true);
        require(sender.unlocked_balance(0, true) > COIN, "Mature coinbase funds were not scanned");
        std::vector<tx_destination_entry> destinations{{COIN, recipient_keys.m_account_address, false}};
        auto transfers = sender.create_transactions_2(destinations, 0, tools::fee_priority::Unimportant, {}, 0, {});
        require(!transfers.empty(), "Wallet produced no FCMP transaction");
        for (const auto &transfer : transfers)
        {
            require(rct::is_rct_fcmp(transfer.tx.rct_signatures.type), "Wallet did not construct an FCMP transaction");
            std::cout << "fcmp_transaction_bytes=" << tx_to_blob(transfer.tx).size()
                << " fee_atomic=" << transfer.fee << std::endl;
        }
        verify_restricted_templates(miner, address, transfers);
        sender.commit_tx(transfers);
        std::cout << "fcmp_transaction_relayed=true" << std::endl;
        COMMAND_RPC_GET_TRANSACTIONS::request lookup{};
        lookup.prune = true;
        for (const auto &transfer : transfers)
            lookup.txs_hashes.push_back(epee::string_tools::pod_to_hex(get_transaction_hash(transfer.tx)));
        miner.mining(address, true);
        const auto payment_deadline = std::chrono::steady_clock::now() + 180s;
        auto next_report = std::chrono::steady_clock::now();
        for (;;)
        {
            require(std::chrono::steady_clock::now() < payment_deadline, "Payment inclusion or maturity timed out");
            COMMAND_RPC_GET_TRANSACTIONS::response transactions{};
            require(epee::net_utils::invoke_http_json("/get_transactions", lookup, transactions, miner.rpc, 10s)
                && transactions.status == CORE_RPC_STATUS_OK, "Unable to observe the relayed payment");
            require(miner.get_info(mined), "Unable to read the payment confirmation height");
            const bool mature = std::all_of(lookup.txs_hashes.begin(), lookup.txs_hashes.end(), [&](const std::string &hash) {
                return std::any_of(transactions.txs.begin(), transactions.txs.end(), [&](const auto &tx) {
                    return tx.tx_hash == hash && !tx.in_pool &&
                        mined.height > tx.block_height + CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE;
                });
            });
            if (mature)
                break;
            if (std::chrono::steady_clock::now() >= next_report)
            {
                std::cout << "payment_confirmation_pending height=" << mined.height << std::endl;
                next_report = std::chrono::steady_clock::now() + 20s;
            }
            std::this_thread::sleep_for(500ms);
        }
        miner.mining(address, false);
        require(miner.get_info(mined), "Miner RPC failed after payment confirmation");
        wait_height(observer, mined.height);
        receiver.refresh(true);
        require(receiver.balance(0, true) == COIN, "Recipient did not receive the exact payment");
        require(receiver.unlocked_balance(0, true) == COIN, "Recipient unlocked payment amount differs");
        verify_miner_proof_enforcement(miner, address, miner_account);
        require(miner.get_info(mined), "Unable to read height after the authorized control block");
        wait_height(observer, mined.height);
        observer.stop();
        observer.start(argv[1], data / "observer", 49890, 49880);
        require(observer.get_info(observed) && observed.top_block_hash == mined.top_block_hash,
            "Observer restart did not recover the accepted chain");
        std::cout << "fcmp_payment_verified=true restart_verified=true height=" << mined.height << std::endl;
        verify_reorganization(miner, observer, argv[1], data, address, miner_account, receiver);
        receiver.refresh(true);
        require(receiver.unlocked_balance(0, true) == COIN, "Reorganization changed the settled recipient balance");
        std::vector<tx_destination_entry> return_destinations{{COIN / 2, keys.m_account_address, false}};
        auto returned = receiver.create_transactions_2(return_destinations, 0, tools::fee_priority::Unimportant, {}, 0, {});
        require(!returned.empty(), "No FCMP payment was constructed after reorganization");
        for (const auto &transfer : returned)
            require(rct::is_rct_fcmp(transfer.tx.rct_signatures.type), "Post-reorganization spend did not use FCMP");
        receiver.commit_tx(returned);
        require(miner.get_info(mined), "Unable to read post-reorganization height");
        miner.mining(address, true);
        const auto recovery_deadline = std::chrono::steady_clock::now() + 180s;
        for (;;)
        {
            require(std::chrono::steady_clock::now() < recovery_deadline, "Post-reorganization payment confirmation timed out");
            COMMAND_RPC_GET_TRANSACTIONS::request req{};
            COMMAND_RPC_GET_TRANSACTIONS::response res{};
            req.prune = true;
            for (const auto &transfer : returned)
                req.txs_hashes.push_back(epee::string_tools::pod_to_hex(get_transaction_hash(transfer.tx)));
            require(epee::net_utils::invoke_http_json("/get_transactions", req, res, miner.rpc, 10s)
                && res.status == CORE_RPC_STATUS_OK, "Unable to observe the post-reorganization payment");
            const bool confirmed = std::all_of(req.txs_hashes.begin(), req.txs_hashes.end(), [&](const auto &hash) {
                return std::any_of(res.txs.begin(), res.txs.end(), [&](const auto &tx) {
                    return tx.tx_hash == hash && !tx.in_pool;
                });
            });
            if (confirmed)
                break;
            std::this_thread::sleep_for(500ms);
        }
        miner.mining(address, false);
        require(miner.get_info(mined), "Unable to read final chain height");
        wait_height(observer, mined.height);
        std::cout << "post_reorganization_fcmp_spend_confirmed=true height=" << mined.height << std::endl;
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Local test failed: " << error.what() << std::endl;
        return 1;
    }
}
