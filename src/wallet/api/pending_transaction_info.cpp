#include "pending_transaction_info.h"
#include "transaction_construction_info.h"

using namespace std;

namespace Monero {

    PendingTransactionInfo::~PendingTransactionInfo() = default;

    PendingTransactionInfoImpl::PendingTransactionInfoImpl(WalletImpl &wallet, const tools::wallet2::pending_tx & ptx)
            : m_wallet(wallet)
            , m_ptx(ptx)
            , m_constructionData(new TransactionConstructionInfoImpl(wallet, ptx.construction_data))
    {
    }

    PendingTransactionInfoImpl::~PendingTransactionInfoImpl() = default;

    uint64_t PendingTransactionInfoImpl::fee() const
    {
        return m_ptx.fee;
    }

    uint64_t PendingTransactionInfoImpl::dust() const
    {
        return m_ptx.dust;
    }

    bool PendingTransactionInfoImpl::dustAddedToFee() const
    {
        return m_ptx.dust_added_to_fee;
    }

    std::string PendingTransactionInfoImpl::txKey() const
    {
        return epee::string_tools::pod_to_hex(unwrap(unwrap(m_ptx.tx_key)));
    }

    TransactionConstructionInfo * PendingTransactionInfoImpl::constructionData() const {
        return m_constructionData;
    }

//    TransactionConstructionInfo::Output TransactionConstructionInfoImpl::change() const {
//        return Output(
//                {m_ptx.change_dts.amount, m_ptx.change_dts.address(m_wallet.m_wallet->nettype(), crypto::hash())});
//    }
}

namespace Bitmonero = Monero;