#ifndef FEATHER_TRANSACTION_CONSTRUCTION_INFO_H
#define FEATHER_TRANSACTION_CONSTRUCTION_INFO_H

#include "wallet/api/wallet2_api.h"
#include "wallet/wallet2.h"
#include "wallet.h"
#include <string>

namespace Monero {

class TransactionConstructionInfoImpl : public TransactionConstructionInfo
{
public:
    TransactionConstructionInfoImpl(WalletImpl &wallet, const tools::wallet2::tx_construction_data & ptx);
    ~TransactionConstructionInfoImpl() override;

    uint64_t unlockTime() const override;
    std::set<std::uint32_t> subaddressIndices() const override;
    std::vector<std::string> subaddresses() const override;
    uint64_t minMixinCount() const override;
    std::vector<Input> inputs() const override;
    std::vector<Output> outputs() const override;

private:
    friend class WalletImpl;
    WalletImpl &m_wallet;
    tools::wallet2::tx_construction_data m_txcd;
};

}

#endif //FEATHER_TRANSACTION_CONSTRUCTION_INFO_H
