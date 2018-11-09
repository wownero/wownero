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

#include "message_store.h"
#include <boost/archive/portable_binary_oarchive.hpp>
#include <boost/archive/portable_binary_iarchive.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <sstream>
#include "file_io_utils.h"
#include "storages/http_abstract_invoke.h"
#include "wallet_errors.h"
#include "serialization/binary_utils.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "wallet.mms"

namespace mms
{
static std::string get_human_readable_timestamp(uint64_t ts)
{
  char buffer[64];
  time_t tt = ts;
  struct tm tm;
#ifdef WIN32
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
  return std::string(buffer);
}

message_store::message_store() {
  m_active = false;
  m_auto_send = false;
  m_next_message_id = 1;
  m_coalition_size = 0;
  m_threshold = 0;
  m_nettype = cryptonote::network_type::UNDEFINED;
  m_run = true;
}

namespace
{
  // MMS options handling mirrors what "wallet2" is doing for its options, on-demand init and all
  // It's not very clean to initialize Bitmessage-specific options here, but going one level further
  // down still into "message_transporter" for that is a little bit too much
  struct options {
    const command_line::arg_descriptor<std::string> bitmessage_address = {"bitmessage-address", mms::message_store::tr("Use PyBitmessage instance at URL <arg>"), "http://localhost:8442/"};
    const command_line::arg_descriptor<std::string> bitmessage_login = {"bitmessage-login", mms::message_store::tr("Specify <arg> as username:password for PyBitmessage API"), "username:password"};
  };
}

void message_store::init_options(boost::program_options::options_description& desc_params)
{
  const options opts{};
  command_line::add_arg(desc_params, opts.bitmessage_address);
  command_line::add_arg(desc_params, opts.bitmessage_login);
}

void message_store::init(const multisig_wallet_state &state, const std::string &own_label,
			 const std::string &own_transport_address, uint32_t coalition_size, uint32_t threshold)
{
  m_coalition_size = coalition_size;
  m_threshold = threshold;
  m_members.clear();
  m_messages.clear();
  m_next_message_id = 1;

  coalition_member member;
  member.label.clear();
  member.transport_address.clear();
  member.monero_address_known = false;
  memset(&member.monero_address, 0, sizeof(cryptonote::account_public_address));
  member.index = 0;

  for (size_t i = 0; i < m_coalition_size; ++i) {
    member.me = member.index == 0;    // Simple convention/automatism for now: The very first member is fixed as / must be "me"
    m_members.push_back(member);
    member.index++;
  }

  set_member(state, 0, own_label, own_transport_address, state.address);

  m_nettype = state.nettype;
  set_active(true);
  m_filename = state.mms_file;
  save(state);
}

void message_store::set_options(const boost::program_options::variables_map& vm)
{
  const options opts{};
  auto bitmessage_address = command_line::get_arg(vm, opts.bitmessage_address);
  auto bitmessage_login = command_line::get_arg(vm, opts.bitmessage_login);
  set_options(bitmessage_address, bitmessage_login);
}

void message_store::set_options(const std::string &bitmessage_address, const std::string &bitmessage_login)
{
  m_transporter.set_options(bitmessage_address, bitmessage_login);
}

void message_store::set_member(const multisig_wallet_state &state,
			       uint32_t index,
                               const boost::optional<std::string> &label,
                               const boost::optional<std::string> &transport_address,
                               const boost::optional<cryptonote::account_public_address> monero_address)
{
  THROW_WALLET_EXCEPTION_IF(index >= m_coalition_size, tools::error::wallet_internal_error, "Invalid member index " + index);
  coalition_member &m = m_members[index];
  if (label)
  {
    m.label = label.get();
  }
  if (transport_address)
  {
    m.transport_address = transport_address.get();
  }
  if (monero_address)
  {
    m.monero_address_known = true;
    m.monero_address = monero_address.get();
  }
  // Save to minimize the chance to loose that info (at least while in beta)
  save(state);
}

const coalition_member &message_store::get_member(uint32_t index) const
{
  return m_members[index];
}

bool message_store::member_info_complete() const
{
  for (size_t i = 0; i < m_members.size(); ++i) {
    const coalition_member &m = m_members[i];
    if (m.label.empty() || m.transport_address.empty() || !m.monero_address_known) {
      return false;
    }
  }
  return true;
}

bool message_store::get_member_index_by_monero_address(const cryptonote::account_public_address &monero_address, uint32_t &index) const
{
  for (size_t i = 0; i < m_members.size(); ++i) {
    const coalition_member &m = m_members[i];
    if (m.monero_address == monero_address) {
      index = m.index;
      return true;
    }
  }
  MWARNING("No coalition member with Monero address " << account_address_to_string(monero_address));
  return false;
}

bool message_store::get_member_index_by_label(const std::string label, uint32_t &index) const
{
  for (size_t i = 0; i < m_members.size(); ++i) {
    const coalition_member &m = m_members[i];
    if (m.label == label) {
      index = m.index;
      return true;
    }
  }
  MWARNING("No coalition member with label " << label);
  return false;
}

void message_store::process_wallet_created_data(const multisig_wallet_state &state, message_type type, const std::string &content) {
  switch(type)
  {
  case message_type::key_set:
    // Result of a "prepare_multisig" command in the wallet
    // Send the key set to all other members
  case message_type::finalizing_key_set:
    // Result of a "make_multisig" command in the wallet in case of N-1/N multisig
    // Send the finalizing key set to all other members
  case message_type::multisig_sync_data:
    // Result of a "export_multisig_info" command in the wallet
    // Send the sync data to all other members
    for (size_t i = 1; i < m_members.size(); ++i) {
      add_message(state, i, type, message_direction::out, content);
    }
    break;

  case message_type::partially_signed_tx:
    // Result of a "transfer" command in the wallet, or a "sign_multisig" command
    // that did not yet result in the minimum number of signatures required
    // Create a message "from me to me" as a container for the tx data
    if (m_threshold == 1)
    {
      // Probably rare, but possible: The 1 signature is already enough, correct the type
      // Easier to correct here than asking all callers to detect this rare special case
      type = message_type::fully_signed_tx;
    }
    add_message(state, 0, type, message_direction::in, content);
    break;

  case message_type::fully_signed_tx:
    add_message(state, 0, type, message_direction::in, content);
    break;

  default:
    THROW_WALLET_EXCEPTION(tools::error::wallet_internal_error, "Illegal message type " + (uint32_t)type);
    break;
  }
}

uint32_t message_store::add_message(const multisig_wallet_state &state,
                                    uint32_t member_index, message_type type, message_direction direction,
				    const std::string &content)
{
  message m;
  m.id = m_next_message_id++;
  m.type = type;
  m.direction = direction;
  m.content = content;
  m.created = time(NULL);
  m.modified = m.created;
  m.sent = 0;
  m.member_index = member_index;
  if (direction == message_direction::out)
  {
    m.state = message_state::ready_to_send;
  }
  else {
    m.state = message_state::waiting;
  };
  m.wallet_height = state.num_transfer_details;
  m.round = 0;  // Future expansion for fully generalized M/N multisig
  m.signature_count = 0;  // Future expansion for signature counting when signing txs
  m.hash = crypto::null_hash;
  m_messages.push_back(m);

  // Save for every new message right away (at least while in beta)
  save(state);

  uint32_t id = m_messages.size() - 1;
  MINFO(boost::format("Added %s message %s for member %s of type %s")
	  % message_direction_to_string(direction) % id % member_index % message_type_to_string(type));
  return id;
}

// Get the index of the message with id "id", return false if not found
bool message_store::get_message_index_by_id(uint32_t id, uint32_t &index) const
{
  for (size_t i = 0; i < m_messages.size(); ++i)
  {
    if (m_messages[i].id == id)
    {
      index = i;
      return true;
    }
  }
  MWARNING("No message found with an id of " << id);
  return false;
}

// Get the message with id "id" that must exist
uint32_t message_store::get_message_index_by_id(uint32_t id) const
{
  uint32_t index;
  bool found = get_message_index_by_id(id, index);
  THROW_WALLET_EXCEPTION_IF(!found, tools::error::wallet_internal_error, "Invalid message id " + id);
  return index;
}

// Get the message with id "id", return false if not found
// This version of the method allows to check whether id is valid without triggering an error
bool message_store::get_message_by_id(uint32_t id, message &m) const
{
  uint32_t index;
  bool found = get_message_index_by_id(id, index);
  if (found)
  {
    m = m_messages[index];
  }
  return found;
}

// Get the message with id "id" that must exist
message message_store::get_message_by_id(uint32_t id) const
{
  message m;
  bool found = get_message_by_id(id, m);
  THROW_WALLET_EXCEPTION_IF(!found, tools::error::wallet_internal_error, "Invalid message id " + id);
  return m;
}

bool message_store::any_message_of_type(message_type type, message_direction direction) const
{
  for (size_t i = 0; i < m_messages.size(); ++i)
  {
    if ((m_messages[i].type == type) && (m_messages[i].direction == direction))
    {
      return true;
    }
  }
  return false;
}

bool message_store::any_message_with_hash(const crypto::hash &hash) const
{
  for (size_t i = 0; i < m_messages.size(); ++i)
  {
    if (m_messages[i].hash == hash)
    {
      return true;
    }
  }
  return false;
}

// Is in every element of vector 'ids' (except at index 0) a message id i.e. not 0?
// Mostly used to check whether we have a message for each coalition member except me,
// with the member index used as index into 'ids'; the element at index 0, for me,
// is ignored, to make constant subtractions of 1 for indices when filling the
// vector unnecessary
bool message_store::message_ids_complete(const std::vector<uint32_t> ids) const
{
  for (size_t i = 1 /* and not 0 */; i < ids.size(); ++i)
  {
    if (ids[i] == 0)
    {
      return false;
    }
  }
  return true;
}

void message_store::delete_message(uint32_t id)
{
  delete_transport_message(id);
  uint32_t index = get_message_index_by_id(id);
  m_messages.erase(m_messages.begin() + index);
}

void message_store::delete_all_messages()
{
  for (size_t i = 0; i < m_messages.size(); ++i)
  {
    delete_transport_message(m_messages[i].id);
  }
  m_messages.clear();
}

void message_store::write_to_file(const multisig_wallet_state &state, const std::string &filename) {
  std::stringstream oss;
  boost::archive::portable_binary_oarchive ar(oss);
  try
  {
    ar << *this;
  }
  catch (...)
  {
    THROW_WALLET_EXCEPTION_IF(true, tools::error::file_save_error, filename);
  }
  std::string buf = oss.str();

  crypto::chacha_key key;
  crypto::generate_chacha_key(&state.view_secret_key, sizeof(crypto::secret_key), key);

  file_data write_file_data = boost::value_initialized<file_data>();
  write_file_data.magic_string = "MMS";
  write_file_data.file_version = 0;
  write_file_data.iv = crypto::rand<crypto::chacha_iv>();
  std::string encrypted_data;
  encrypted_data.resize(buf.size());
  crypto::chacha20(buf.data(), buf.size(), key, write_file_data.iv, &encrypted_data[0]);
  write_file_data.encrypted_data = encrypted_data;

  std::stringstream file_oss;
  boost::archive::portable_binary_oarchive file_ar(file_oss);
  try
  {
    file_ar << write_file_data;
  }
  catch (...)
  {
    THROW_WALLET_EXCEPTION_IF(true, tools::error::file_save_error, filename);
  }

  bool success = epee::file_io_utils::save_string_to_file(filename, file_oss.str());
  THROW_WALLET_EXCEPTION_IF(!success, tools::error::file_save_error, filename);
}

void message_store::read_from_file(const multisig_wallet_state &state, const std::string &filename)
{
  boost::system::error_code ignored_ec;
  bool file_exists = boost::filesystem::exists(filename, ignored_ec);
  if (!file_exists) {
    // Simply do nothing if the file is not there; allows e.g. easy recovery
    // from problems with the MMS by deleting the file
    MERROR("No message store file found: " << filename);
    return;
  }

  std::string buf;
  bool success = epee::file_io_utils::load_file_to_string(filename, buf);
  THROW_WALLET_EXCEPTION_IF(!success, tools::error::file_read_error, filename);

  file_data read_file_data;
  try
  {
    std::stringstream iss;
    iss << buf;
    boost::archive::portable_binary_iarchive ar(iss);
    ar >> read_file_data;
  }
  catch (const std::exception &e)
  {
    MERROR("MMS file " << filename << " has bad structure <iv,encrypted_data>: " << e.what());
    THROW_WALLET_EXCEPTION_IF(true, tools::error::file_read_error, filename);
  }

  crypto::chacha_key key;
  crypto::generate_chacha_key(&state.view_secret_key, sizeof(crypto::secret_key), key);
  std::string decrypted_data;
  decrypted_data.resize(read_file_data.encrypted_data.size());
  crypto::chacha20(read_file_data.encrypted_data.data(), read_file_data.encrypted_data.size(), key, read_file_data.iv, &decrypted_data[0]);

  try
  {
    std::stringstream iss;
    iss << decrypted_data;
    boost::archive::portable_binary_iarchive ar(iss);
    ar >> *this;
  }
  catch (const std::exception &e)
  {
    MERROR("MMS file " << filename << " has bad structure: " << e.what());
    THROW_WALLET_EXCEPTION_IF(true, tools::error::file_read_error, filename);
  }

  m_filename = filename;
}

// Save to the same file this message store was loaded from
// Called after changes deemed "important", to make it less probable to loose messages in case of
// a crash; a better and long-term solution would of course be to use LMDB ...
void message_store::save(const multisig_wallet_state &state)
{
  if (!m_filename.empty())
  {
    write_to_file(state, m_filename);
  }
}

bool message_store::get_processable_messages(const multisig_wallet_state &state,
                                             bool force_sync, std::vector<processing_data> &data_list, std::string &wait_reason)
{
  uint32_t wallet_height = state.num_transfer_details;
  data_list.clear();
  wait_reason.clear();
  if (m_members.size() < m_coalition_size)
  {
    // Unless ALL members are known we can't do anything
    wait_reason = tr("The list of coalition members is not complete.");
    return false;
  }

  if (!state.multisig)
  {
    if (!any_message_of_type(message_type::key_set, message_direction::out))
    {
      // With the own key set not yet ready we must do "prepare_multisig" first;
      // Key sets from other members may be here already, but if we process them now
      // the wallet will go multisig too early: we can't produce our own key set any more!
      processing_data data;
      data.processing = message_processing::prepare_multisig;
      data_list.push_back(data);
      return true;
    }

    // Ids of key set messages per member index, to check completeness
    // Naturally, does not care about the order of the messages and is trivial to secure against
    // key sets that were received more than once
    std::vector<uint32_t> key_set_messages(m_coalition_size, 0);

    for (size_t i = 0; i < m_messages.size(); ++i)
    {
      message &m = m_messages[i];
      if ((m.type == message_type::key_set) && (m.state == message_state::waiting))
      {
	if (key_set_messages[m.member_index] == 0)
	{
	  key_set_messages[m.member_index] = m.id;
	}
	// else duplicate key set, ignore
      }
    }

    bool key_sets_complete = message_ids_complete(key_set_messages);
    if (key_sets_complete)
    {
      // Nothing else can be ready to process earlier than this, ignore everything else and give back
      processing_data data;
      data.processing = message_processing::make_multisig;
      data.message_ids = key_set_messages;
      data.message_ids.erase(data.message_ids.begin());
      data_list.push_back(data);
      return true;
    }
    else
    {
      wait_reason = tr("Wallet can't go multisig because key sets from other members missing or not complete.");
      return false;
    }
  }

  if (state.multisig && !state.multisig_is_ready)
  {
    // In the case of N-1/N multisig the call 'wallet2::multisig' returns already true
    // after "make_multisig" but before "finalize_multisig", but returns the parameter
    // 'ready' as false to document this particular state

    // Same story for finalizing key sets: If all are here we process them
    // It looks like the "finalize_multisig" command would also process less than all key sets,
    // and maybe also correctly so, but the MMS does not support that case and insists on completeness
    std::vector<uint32_t> finalizing_key_set_messages(m_coalition_size, 0);

    for (size_t i = 0; i < m_messages.size(); ++i)
    {
      message &m = m_messages[i];
      if ((m.type == message_type::finalizing_key_set) && (m.state == message_state::waiting))
      {
	if (finalizing_key_set_messages[m.member_index] == 0)
	{
	  finalizing_key_set_messages[m.member_index] = m.id;
	}
	// else duplicate key set, ignore
      }
    }

    bool key_sets_complete = message_ids_complete(finalizing_key_set_messages);
    if (key_sets_complete)
    {
      processing_data data;
      data.processing = message_processing::finalize_multisig;
      data.message_ids = finalizing_key_set_messages;
      data.message_ids.erase(data.message_ids.begin());
      data_list.push_back(data);
      return true;
    }
    else
    {
      wait_reason = tr("Wallet can't finalize multisig because key sets from other members missing or not complete.");
      return false;
    }
  }

  // Properly exchanging multisig sync data is easiest and most transparent
  // for the user if a wallet sends its own data first and processes any received
  // sync data afterwards so that's the order that the MMS enforces here.
  // (Technically, it seems to work also the other way round.)
  //
  // To check whether a NEW round of syncing is necessary the MMS works with a
  // "wallet state": new state means new syncing needed.
  //
  // The MMS monitors the "wallet state" by recording "wallet heights" as
  // numbers of transfers present in a wallet at the time of message creation. While
  // not watertight, this quite simple scheme should already suffice to trigger
  // and orchestrate a sensible exchange of sync data.
  if (state.has_multisig_partial_key_images || force_sync)
  {
    // Sync is necessary and not yet completed: Processing of transactions
    // will only be possible again once properly synced
    // Check first whether we generated already OUR sync info; take note of
    // any processable sync info from other members on the way in case we need it
    bool own_sync_data_created = false;
    std::vector<uint32_t> sync_messages(m_coalition_size, 0);
    for (size_t i = 0; i < m_messages.size(); ++i)
    {
      message &m = m_messages[i];
      if ((m.type == message_type::multisig_sync_data) && (force_sync || (m.wallet_height == wallet_height)))
      // It's data for the same "round" of syncing, on the same "wallet height", therefore relevant
      {
	if (m.direction == message_direction::out)
	{
	  own_sync_data_created = true;
	  // Ignore whether sent already or not, and assume as complete if several other members there
	}
	else if ((m.direction == message_direction::in) && (m.state == message_state::waiting))
	{
	  if (sync_messages[m.member_index] == 0)
	  {
	    sync_messages[m.member_index] = m.id;
	  }
	  // else duplicate sync message, ignore
	}
      }
    }
    if (!own_sync_data_created) {
      // As explained above, creating sync data BEFORE processing such data from
      // other members reliably works, so insist on that here
      processing_data data;
      data.processing = message_processing::create_sync_data;
      data_list.push_back(data);
      return true;
    }
    else if (message_ids_complete(sync_messages))
    {
      processing_data data;
      data.processing = message_processing::process_sync_data;
      data.message_ids = sync_messages;
      data.message_ids.erase(data.message_ids.begin());
      data_list.push_back(data);
      return true;
    }
    else
    {
      // We can't proceed to any transactions until we have synced
      wait_reason = tr("Syncing not possible because multisig sync data from other members missing or not complete.");
      return false;
    }
  }

  bool waiting_found = false;
  for (size_t i = 0; i < m_messages.size(); ++i)
  {
    message &m = m_messages[i];
    if (m.state == message_state::waiting)
    {
      waiting_found = true;
      if (m.type == message_type::fully_signed_tx)
      {
	// We can either submit it ourselves, or send it to any other member for submission
	processing_data data;
	data.processing = message_processing::submit_tx;
	data.message_ids.push_back(m.id);
	data_list.push_back(data);

	data.processing = message_processing::send_tx;
	for (size_t j = 1; j < m_members.size(); ++j)
	{
	  data.receiving_member_index = j;
	  data_list.push_back(data);
	}
	return true;
      }
      else if (m.type == message_type::partially_signed_tx)
      {
	if (m.member_index == 0)
	{
	  // We started this ourselves, or signed it but with still signatures missing:
	  // We can send it to any other member for signing / further signing
	  // In principle it does not make sense to send it back to somebody who
	  // already signed, but the MMS does not / not yet keep track of that,
	  // because that would be somewhat complicated.
	  processing_data data;
	  data.processing = message_processing::send_tx;
	  data.message_ids.push_back(m.id);
	  for (size_t j = 1; j < m_members.size(); ++j)
	  {
	    data.receiving_member_index = j;
	    data_list.push_back(data);
	  }
	  return true;
	}
	else
	{
	  // Somebody else sent this to us: We can sign it
	  // It would be possible to just pass it on, but that's not directly supported here
	  processing_data data;
	  data.processing = message_processing::sign_tx;
	  data.message_ids.push_back(m.id);
	  data_list.push_back(data);
	  return true;
	}
      }
    }
  }
  if (waiting_found)
  {
    wait_reason = tr("Waiting message is not a tx and thus not processable now.");
  }
  else
  {
    wait_reason = tr("There is no message waiting to be processed.");
  }

  return false;
}

void message_store::set_messages_processed(const processing_data &data)
{
  for (size_t i = 0; i < data.message_ids.size(); ++i)
  {
    set_message_processed_or_sent(data.message_ids[i]);
  }
}

void message_store::set_message_processed_or_sent(uint32_t id)
{
  uint32_t index = get_message_index_by_id(id);
  message &m = m_messages[index];
  if (m.state == message_state::waiting)
  {
    // So far a fairly cautious and conservative strategy: Only delete from Bitmessage
    // when fully processed (and e.g. not already after reception and writing into
    // the message store file)
    delete_transport_message(id);
    m.state = message_state::processed;
  }
  else if (m.state == message_state::ready_to_send)
  {
    m.state = message_state::sent;
  }
  m.modified = time(NULL);
}

void message_store::encrypt(uint32_t member_index, const std::string &plaintext,
			    std::string &ciphertext, crypto::public_key &encryption_public_key, crypto::chacha_iv &iv)
{
  crypto::secret_key encryption_secret_key;
  crypto::generate_keys(encryption_public_key, encryption_secret_key);

  crypto::key_derivation derivation;
  crypto::public_key dest_view_public_key = m_members[member_index].monero_address.m_view_public_key;
  bool success = crypto::generate_key_derivation(dest_view_public_key, encryption_secret_key, derivation);
  THROW_WALLET_EXCEPTION_IF(!success, tools::error::wallet_internal_error, "Failed to generate key derivation for message encryption");

  crypto::chacha_key chacha_key;
  crypto::generate_chacha_key(&derivation, sizeof(derivation), chacha_key);
  iv = crypto::rand<crypto::chacha_iv>();
  ciphertext.resize(plaintext.size());
  crypto::chacha20(plaintext.data(), plaintext.size(), chacha_key, iv, &ciphertext[0]);
}

void message_store::decrypt(const std::string &ciphertext, const crypto::public_key &encryption_public_key, const crypto::chacha_iv &iv,
			    const crypto::secret_key &view_secret_key, std::string &plaintext)
{
  crypto::key_derivation derivation;
  bool success = crypto::generate_key_derivation(encryption_public_key, view_secret_key, derivation);
  THROW_WALLET_EXCEPTION_IF(!success, tools::error::wallet_internal_error, "Failed to generate key derivation for message decryption");
  crypto::chacha_key chacha_key;
  crypto::generate_chacha_key(&derivation, sizeof(derivation), chacha_key);
  plaintext.resize(ciphertext.size());
  crypto::chacha20(ciphertext.data(), ciphertext.size(), chacha_key, iv, &plaintext[0]);
}

void message_store::send_message(const multisig_wallet_state &state, uint32_t id)
{
  uint32_t index = get_message_index_by_id(id);
  message m = m_messages[index];
  file_transport_message dm;
  dm.sender_address = m_members[0].monero_address;
  dm.internal_message = m;
  encrypt(m.member_index, m.content, dm.internal_message.content, dm.encryption_public_key, dm.iv);
  std::string transport_address = m_members[m.member_index].transport_address;
  if (transport_address.find("BM-") == 0)
  {
    // Take the transport address of the member as Bitmessage address and use the message transporter
    transport_message rm;
    rm.source_monero_address = dm.sender_address;
    rm.source_transport_address = m_members[0].transport_address;
    rm.destination_monero_address = m_members[m.member_index].monero_address;
    rm.destination_transport_address = transport_address;
    rm.iv = dm.iv;
    rm.encryption_public_key = dm.encryption_public_key;
    rm.timestamp = time(NULL);
    rm.type = (uint32_t)dm.internal_message.type;
    rm.subject = "MMS V0 " + get_human_readable_timestamp(rm.timestamp);
    rm.content = dm.internal_message.content;
    rm.hash = crypto::cn_fast_hash(rm.content.data(), rm.content.size());

    crypto::generate_signature(rm.hash, m_members[0].monero_address.m_view_public_key, state.view_secret_key, rm.signature);

    bool success = m_transporter.send_message(rm);
  }
  else
  {
    // Take the transport address of the member as a subdirectory and write the message as
    // file "debug_message" there
    std::string filename = transport_address + "/debug_message";

    std::stringstream oss;
    boost::archive::portable_binary_oarchive ar(oss);
    ar << dm;
    std::string buf = oss.str();
    bool r = epee::file_io_utils::save_string_to_file(filename, buf);
  }

  m_messages[index].state=message_state::sent;
  m_messages[index].sent= time(NULL);
}

bool message_store::check_for_messages(const multisig_wallet_state &state, std::vector<message> &messages)
{
  m_run.store(true, std::memory_order_relaxed);
  std::string transport_address = m_members[0].transport_address;
  if (transport_address.find("BM-") == 0)
  {
      // Take the transport address of "me" as Bitmessage address and use the messaging daemon
    std::vector<transport_message> transport_messages;
    bool r = m_transporter.receive_messages(m_members[0].monero_address, transport_address, transport_messages);
    if (!m_run.load(std::memory_order_relaxed))
    {
      // Stop was called, don't waste time processing the messages
      // (but once started processing them, don't react to stop request anymore, avoid receiving them "partially)"
      return false;
    }

    bool new_messages = false;
    for (size_t i = 0; i < transport_messages.size(); ++i)
    {
      transport_message rm = transport_messages[i];
      if (any_message_with_hash(rm.hash))
      {
	// Already seen, do not take again
      }
      else
      {
        uint32_t sender_index;
	bool found = get_member_index_by_monero_address(rm.source_monero_address, sender_index);
	if (!found)
	{
	  // From an address that is not a member here: Ignore
	}
	else
	{
	  crypto::hash actual_hash = crypto::cn_fast_hash(rm.content.data(), rm.content.size());
          THROW_WALLET_EXCEPTION_IF(actual_hash != rm.hash, tools::error::wallet_internal_error, "Message hash mismatch");

	  bool signature_valid = crypto::check_signature(actual_hash, rm.source_monero_address.m_view_public_key, rm.signature);
          THROW_WALLET_EXCEPTION_IF(!signature_valid, tools::error::wallet_internal_error, "Message signature not valid");

	  std::string plaintext;
	  decrypt(rm.content, rm.encryption_public_key, rm.iv, state.view_secret_key, plaintext);
	  uint32_t index = add_message(state, sender_index, (message_type)rm.type, message_direction::in, plaintext);
	  message &m = m_messages[index];
	  m.hash = rm.hash;
	  m.transport_id = rm.transport_id;
	  m.sent = rm.timestamp;
	  m.round = rm.round;
	  m.signature_count = rm.signature_count;
	  messages.push_back(m);
	  new_messages = true;
	}
      }
    }
    return new_messages;
  }
  else
  {
    // Take the transport address of "me" as a subdirectory and read a single new
    // message from a file "debug_message" there, assumed to come from member 1
    std::string filename = m_members[0].transport_address + "/debug_message";
    boost::system::error_code ignored_ec;
    bool file_exists = boost::filesystem::exists(filename, ignored_ec);
    if (!file_exists) {
      return false;
    }

    file_transport_message dm;
    std::string buf;
    bool r = epee::file_io_utils::load_file_to_string(filename, buf);
    std::stringstream iss;
    iss << buf;
    boost::archive::portable_binary_iarchive ar(iss);
    ar >> dm;

    uint32_t sender_index;
    get_member_index_by_monero_address(dm.sender_address, sender_index);
    std::string plaintext;
    decrypt(dm.internal_message.content, dm.encryption_public_key, dm.iv, state.view_secret_key, plaintext);
    add_message(state, sender_index, dm.internal_message.type, message_direction::in, plaintext);
    boost::filesystem::remove(filename);
    return true;
  }
  return false;
}

void message_store::delete_transport_message(uint32_t id)
{
  uint32_t index = get_message_index_by_id(id);
  message m = m_messages[index];
  if (!m.transport_id.empty())
  {
    m_transporter.delete_message(m.transport_id);
  }
}

std::string message_store::account_address_to_string(const cryptonote::account_public_address &account_address) const
{
  return get_account_address_as_str(m_nettype, false, account_address);
}

const char* message_store::message_type_to_string(message_type type) {
  switch (type)
  {
  case message_type::key_set:
    return tr("key set");
  case message_type::finalizing_key_set:
    return tr("finalizing key set");
  case message_type::multisig_sync_data:
    return tr("multisig sync data");
  case message_type::partially_signed_tx:
    return tr("partially signed tx");
  case message_type::fully_signed_tx:
    return tr("fully signed tx");
  case message_type::note:
    return tr("note");
  default:
    return tr("unknown message type");
  }
}

const char* message_store::message_direction_to_string(message_direction direction) {
  switch (direction)
  {
  case message_direction::in:
    return tr("in");
  case message_direction::out:
    return tr("out");
  default:
    return tr("unknown message direction");
  }
}

const char* message_store::message_state_to_string(message_state state) {
  switch (state)
  {
  case message_state::ready_to_send:
    return tr("ready to send");
  case message_state::sent:
    return tr("sent");
  case message_state::waiting:
    return tr("waiting");
  case message_state::processed:
    return tr("processed");
  case message_state::cancelled:
    return tr("cancelled");
  default:
    return tr("unknown message state");
  }
}

// Convert a member to string suitable for a column in a list, with 'max_width'
// Format: label: transport_address
std::string message_store::member_to_string(const coalition_member &member, uint32_t max_width) {
  std::string s = "";
  s.reserve(max_width);
  uint32_t avail = max_width;
  uint32_t label_len = member.label.length();
  if (label_len > avail)
  {
    s.append(member.label.substr(0, avail - 2));
    s.append("..");
    return s;
  }
  s.append(member.label);
  avail -= label_len;
  uint32_t transport_addr_len = member.transport_address.length();
  if ((transport_addr_len > 0) && (avail > 10))
  {
    s.append(": ");
    avail -= 2;
    if (transport_addr_len <= avail)
    {
      s.append(member.transport_address);
    }
    else
    {
      s.append(member.transport_address.substr(0, avail-2));
      s.append("..");
    }
  }
  return s;
}


}
