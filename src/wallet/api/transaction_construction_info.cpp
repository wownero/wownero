#include "transaction_construction_info.h"

using namespace std;

namespace Monero {
    TransactionConstructionInfo::~TransactionConstructionInfo() = default;

    TransactionConstructionInfo::Input::Input(uint64_t _amount, const std::string &_pubkey)
            : amount(_amount), pubkey(_pubkey) {}

    TransactionConstructionInfo::Output::Output(uint64_t _amount, const std::string &_address)
            : amount(_amount), address(_address) {}

    TransactionConstructionInfoImpl::TransactionConstructionInfoImpl(WalletImpl &wallet, const tools::wallet2::tx_construction_data & txcd)
        : m_wallet(wallet)
        , m_txcd(txcd) {}

    TransactionConstructionInfoImpl::~TransactionConstructionInfoImpl() = default;

    uint64_t TransactionConstructionInfoImpl::unlockTime() const {
        return m_txcd.unlock_time;
    }

    std::set<std::uint32_t> TransactionConstructionInfoImpl::subaddressIndices() const {
        return m_txcd.subaddr_indices;
    }

    std::vector<std::string> TransactionConstructionInfoImpl::subaddresses() const {
        std::vector<std::string> s;
        auto major = m_txcd.subaddr_account;
        for (const auto &minor : m_txcd.subaddr_indices) {
            s.push_back(m_wallet.m_wallet->get_subaddress_as_str({major, minor}));
        }
        return s;
    }

    uint64_t TransactionConstructionInfoImpl::minMixinCount() const {
        uint64_t min_mixin = -1;
        for (const auto &source : m_txcd.sources) {
            size_t mixin = source.outputs.size() - 1;
            if (mixin < min_mixin)
                min_mixin = mixin;
        }

        return min_mixin;
    }

    std::vector<TransactionConstructionInfo::Input> TransactionConstructionInfoImpl::inputs() const {
        std::vector<Input> inputs;
        for (const auto &i : m_txcd.sources) {
            inputs.emplace_back(i.amount, epee::string_tools::pod_to_hex(i.outputs[i.real_output].second.dest));
        }
        return inputs;
    }

    std::vector<TransactionConstructionInfo::Output> TransactionConstructionInfoImpl::outputs() const {
        std::vector<Output> outputs;
        for (const auto &o : m_txcd.splitted_dsts) {
            outputs.emplace_back(o.amount, o.address(m_wallet.m_wallet->nettype(), crypto::hash()));
        }
        return outputs;
    }
}
namespace Bitmonero = Monero;