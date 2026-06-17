#ifndef FEATHER_PENDING_TX_H
#define FEATHER_PENDING_TX_H

#include "wallet/api/wallet2_api.h"
#include "wallet/wallet2.h"
#include "wallet.h"
#include <string>

namespace Monero {

class PendingTransactionImpl;

class PendingTransactionInfoImpl : public PendingTransactionInfo
{
public:
    PendingTransactionInfoImpl(WalletImpl &wallet, const tools::wallet2::pending_tx & ptx);
    ~PendingTransactionInfoImpl() override;

    uint64_t fee() const override;
    uint64_t dust() const override;
    bool dustAddedToFee() const override;
    std::string txKey() const override;
    TransactionConstructionInfo *constructionData() const override;
//    Output change() const override;

private:
    friend class WalletImpl;
    WalletImpl &m_wallet;
    tools::wallet2::pending_tx m_ptx;
    TransactionConstructionInfo *m_constructionData;
};

}



#endif //FEATHER_PENDING_TX_H
