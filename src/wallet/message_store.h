// Copyright (c) 2014-2018, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once

#include <cstdlib>
#include <string>
#include <vector>
#include "crypto/hash.h"
#include <boost/serialization/vector.hpp>
#include "serialization/serialization.h"
#include "cryptonote_basic/cryptonote_boost_serialization.h"
#include "cryptonote_basic/account_boost_serialization.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "common/i18n.h"
#include "message_transporter.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "wallet.mms"

namespace mms
{
  enum class message_type
  {
    key_set,
    finalizing_key_set,
    multisig_sync_data,
    partially_signed_tx,
    fully_signed_tx,
    note
  };

  enum class message_direction
  {
    in,
    out
  };

  enum class message_state
  {
    ready_to_send,
    sent,

    waiting,
    processed,

    cancelled
  };

  enum class message_processing
  {
    prepare_multisig,
    make_multisig,
    finalize_multisig,
    create_sync_data,
    process_sync_data,
    sign_tx,
    send_tx,
    submit_tx
  };

  struct message
  {
    uint32_t id;
    message_type type;
    message_direction direction;
    std::string content;
    uint64_t created;
    uint64_t modified;
    uint64_t sent;
    uint32_t member_index;
    crypto::hash hash;
    message_state state;
    uint32_t wallet_height;
    uint32_t round;
    uint32_t signature_count;
    std::string transport_id;
  };
  // "wallet_height" (for lack of a short name that would describe what it is about)
  // is the number of transfers present in the wallet at the time of message
  // construction; used to coordinate generation of sync info (which depends
  // on the content of the wallet at time of generation)

  struct coalition_member
  {
    std::string label;
    std::string transport_address;
    bool monero_address_known;
    cryptonote::account_public_address monero_address;
    bool me;
    uint32_t index;
  };

  struct processing_data
  {
    message_processing processing;
    std::vector<uint32_t> message_ids;
    uint32_t receiving_member_index = 0;
  };

  struct file_transport_message
  {
    cryptonote::account_public_address sender_address;
    crypto::chacha_iv iv;
    crypto::public_key encryption_public_key;
    message internal_message;
  };

  // Overal .mms file structure, with the "message_store" object serialized to and
  // encrypted in "encrypted_data"
  struct file_data
  {
    std::string magic_string;
    uint32_t file_version;
    crypto::chacha_iv iv;
    std::string encrypted_data;
  };

    // The following struct provides info about the current state of a "wallet2" object
  // at the time of a "message_store" method call that those methods need. See on the
  // one hand a first parameter of this type for several of those methods, and on the
  // other hand the method "wallet2::get_multisig_wallet_state" which clients like the
  // CLI wallet can use to get that info.
  //
  // Note that in the case of a wallet that is already multisig "address" is NOT the
  // multisig address, but the "original" wallet address at creation time. Likewise
  // "view_secret_key" is the original view secret key then.
  //
  // This struct definition is here and not in "wallet2.h" to avoid circular imports.
  struct multisig_wallet_state
  {
    cryptonote::account_public_address address;
    cryptonote::network_type nettype;
    crypto::secret_key view_secret_key;
    bool multisig;
    bool multisig_is_ready;
    bool has_multisig_partial_key_images;
    size_t num_transfer_details;
    std::string mms_file;

    ~multisig_wallet_state()
    {
      view_secret_key = crypto::null_skey;
    }
  };

  class message_store
  {
  public:
    message_store();
    // Initialize and start to use the MMS, set the first member, this wallet itself
    // Filename, if not null and not empty, is used to create the ".mms" file
    // reset it if already used, with deletion of all members and messages
    void init(const multisig_wallet_state &state, const std::string &own_label,
              const std::string &own_transport_address, uint32_t coalition_size, uint32_t threshold);
    void set_active(bool active) { m_active = active; };
    void set_auto_send(bool auto_send) { m_auto_send = auto_send; };
    void set_options(const boost::program_options::variables_map& vm);
    void set_options(const std::string &bitmessage_address, const std::string &bitmessage_login);
    bool get_active() const { return m_active; };
    bool get_auto_send() const { return m_auto_send; };
    uint32_t get_threshold() const { return m_threshold; };
    uint32_t get_coalition_size() const { return m_coalition_size; };

    void set_member(const multisig_wallet_state &state,
                    uint32_t index,
                    const boost::optional<std::string> &label,
                    const boost::optional<std::string> &transport_address,
                    const boost::optional<cryptonote::account_public_address> monero_address);

    const coalition_member &get_member(uint32_t index) const;
    bool get_member_index_by_monero_address(const cryptonote::account_public_address &monero_address, uint32_t &index) const;
    bool get_member_index_by_label(const std::string label, uint32_t &index) const;
    const std::vector<coalition_member> &get_all_members() const { return m_members; };
    bool member_info_complete() const;

    // Process data just created by "me" i.e. the own local wallet, e.g. as the result of a "prepare_multisig" command
    // Creates the resulting messages to the right members
    void process_wallet_created_data(const multisig_wallet_state &state, message_type type, const std::string &content);

    // Go through all the messages, look at the "ready to process" ones, and check whether any single one
    // or any group of them can be processed, because they are processable as single messages (like a tx
    // that is fully signed and thus ready for submit to the net) or because they form a complete group
    // (e.g. key sets from all coalition members to make the wallet multisig). If there are multiple
    // candidates, e.g. in 2/3 multisig sending to one OR the other member to sign, there will be more
    // than 1 element in 'data' for the user to choose. If nothing is ready "false" is returned.
    // The method mostly ignores the order in which the messages were received because messages may be delayed
    // (e.g. sync data from a member arrives AFTER a transaction to submit) or because message time stamps
    // may be wrong so it's not possible to order them reliably.
    // Messages also may be ready by themselves but the wallet not yet ready for them (e.g. sync data already
    // arriving when the wallet is not yet multisig because key sets were delayed or were lost altogether.)
    // If nothing is ready 'wait_reason' may contain further info about the reason why.
    bool get_processable_messages(const multisig_wallet_state &state,
                                  bool force_sync,
                                  std::vector<processing_data> &data_list,
                                  std::string &wait_reason);
    void set_messages_processed(const processing_data &data);

    uint32_t add_message(const multisig_wallet_state &state,
                         uint32_t member_index, message_type type, message_direction direction,
                         const std::string &content);
    const std::vector<message> &get_all_messages() const { return m_messages; };
    bool get_message_by_id(uint32_t id, message &m) const;
    message get_message_by_id(uint32_t id) const;
    void set_message_processed_or_sent(uint32_t id);
    void delete_message(uint32_t id);
    void delete_all_messages();

    void send_message(const multisig_wallet_state &state, uint32_t id);
    bool check_for_messages(const multisig_wallet_state &state, std::vector<message> &messages);
    void stop() { m_run.store(false, std::memory_order_relaxed); m_transporter.stop(); }

    void write_to_file(const multisig_wallet_state &state, const std::string &filename);
    void read_from_file(const multisig_wallet_state &state, const std::string &filename);

    template <class t_archive>
    inline void serialize(t_archive &a, const unsigned int ver)
    {
      a & m_active;
      a & m_coalition_size;
      a & m_nettype;
      a & m_threshold;
      a & m_members;
      a & m_messages;
      a & m_next_message_id;
      a & m_auto_send;
    }

    const char* message_type_to_string(message_type type);
    const char* message_direction_to_string(message_direction direction);
    const char* message_state_to_string(message_state state);
    std::string member_to_string(const coalition_member &member, uint32_t max_width);

    static const char *tr(const char *str) { return i18n_translate(str, "tools::mms"); }
    static void init_options(boost::program_options::options_description& desc_params);

  private:
    bool m_active;
    uint32_t m_coalition_size;
    uint32_t m_threshold;
    bool m_auto_send;
    cryptonote::network_type m_nettype;
    std::vector<coalition_member> m_members;
    std::vector<message> m_messages;
    uint32_t m_next_message_id;
    std::string m_filename;
    message_transporter m_transporter;
    std::atomic<bool> m_run;

    bool get_message_index_by_id(uint32_t id, uint32_t &index) const;
    uint32_t get_message_index_by_id(uint32_t id) const;
    bool any_message_of_type(message_type type, message_direction direction) const;
    bool any_message_with_hash(const crypto::hash &hash) const;
    bool message_ids_complete(const std::vector<uint32_t> ids) const;
    void encrypt(uint32_t member_index, const std::string &plaintext,
                 std::string &ciphertext, crypto::public_key &encryption_public_key, crypto::chacha_iv &iv);
    void decrypt(const std::string &ciphertext, const crypto::public_key &encryption_public_key, const crypto::chacha_iv &iv,
                 const crypto::secret_key &view_secret_key, std::string &plaintext);
    void delete_transport_message(uint32_t id);
    std::string account_address_to_string(const cryptonote::account_public_address &account_address) const;
    void save(const multisig_wallet_state &state);
  };
}

BOOST_CLASS_VERSION(mms::file_data, 0)
BOOST_CLASS_VERSION(mms::message_store, 0)
BOOST_CLASS_VERSION(mms::message, 0)
BOOST_CLASS_VERSION(mms::file_transport_message, 0)
BOOST_CLASS_VERSION(mms::coalition_member, 0)

namespace boost
{
  namespace serialization
  {
    template <class Archive>
    inline void serialize(Archive &a, mms::file_data &x, const boost::serialization::version_type ver)
    {
      a & x.magic_string;
      a & x.file_version;
      a & x.iv;
      a & x.encrypted_data;
    }

    template <class Archive>
    inline void serialize(Archive &a, mms::message &x, const boost::serialization::version_type ver)
    {
      a & x.id;
      a & x.type;
      a & x.direction;
      a & x.content;
      a & x.created;
      a & x.modified;
      a & x.sent;
      a & x.member_index;
      a & x.hash;
      a & x.state;
      a & x.wallet_height;
      a & x.round;
      a & x.signature_count;
      a & x.transport_id;
    }

    template <class Archive>
    inline void serialize(Archive &a, mms::coalition_member &x, const boost::serialization::version_type ver)
    {
      a & x.label;
      a & x.transport_address;
      a & x.monero_address_known;
      a & x.monero_address;
      a & x.me;
      a & x.index;
    }

    template <class Archive>
    inline void serialize(Archive &a, mms::file_transport_message &x, const boost::serialization::version_type ver)
    {
      a & x.sender_address;
      a & x.iv;
      a & x.encryption_public_key;
      a & x.internal_message;
    }

    template <class Archive>
    inline void serialize(Archive &a, crypto::chacha_iv &x, const boost::serialization::version_type ver)
    {
      a & x.data;
    }

  }
}