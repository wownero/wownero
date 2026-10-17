// Copyright (c) 2014-2022, The Monero Project
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

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#include <functional>
#include <vector>

using namespace epee;

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
  /**
   * @brief struct for loading a checkpoint from json
   */
  struct t_hashline
  {
    uint64_t height; //!< the height of the checkpoint
    std::string hash; //!< the hash for the checkpoint
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(height)
          KV_SERIALIZE(hash)
        END_KV_SERIALIZE_MAP()
  };

  /**
   * @brief struct for loading many checkpoints from json
   */
  struct t_hash_json {
    std::vector<t_hashline> hashlines; //!< the checkpoint lines from the file
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(hashlines)
        END_KV_SERIALIZE_MAP()
  };

  //---------------------------------------------------------------------------
  checkpoints::checkpoints()
  {
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str, const std::string& difficulty_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    // return false if adding at a height we already have AND the hash is different
    if (m_points.count(height))
    {
      CHECK_AND_ASSERT_MES(h == m_points[height], false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    m_points[height] = h;
    if (!difficulty_str.empty())
    {
      try
      {
        difficulty_type difficulty(difficulty_str);
        if (m_difficulty_points.count(height))
        {
          CHECK_AND_ASSERT_MES(difficulty == m_difficulty_points[height], false, "Difficulty checkpoint at given height already exists, and difficulty for new checkpoint was different!");
        }
        m_difficulty_points[height] = difficulty;
      }
      catch (...)
      {
        LOG_ERROR("Failed to parse difficulty checkpoint: " << difficulty_str);
        return false;
      }
    }
    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    return !m_points.empty() && (height <= (--m_points.end())->first);
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool& is_a_checkpoint) const
  {
    auto it = m_points.find(height);
    is_a_checkpoint = it != m_points.end();
    if(!is_a_checkpoint)
      return true;

    if(it->second == h)
    {
      MINFO("CHECKPOINT PASSED FOR HEIGHT " << height << " " << h);
      return true;
    }else
    {
      MWARNING("CHECKPOINT FAILED FOR HEIGHT " << height << ". EXPECTED HASH: " << it->second << ", FETCHED HASH: " << h);
      return false;
    }
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h) const
  {
    bool ignored;
    return check_block(height, h, ignored);
  }
  //---------------------------------------------------------------------------
  //FIXME: is this the desired behavior?
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const
  {
    if (0 == block_height)
      return false;

    auto it = m_points.upper_bound(blockchain_height);
    // Is blockchain_height before the first checkpoint?
    if (it == m_points.begin())
      return true;

    --it;
    uint64_t checkpoint_height = it->first;
    return checkpoint_height < block_height;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    if (m_points.empty())
      return 0;
    return m_points.rbegin()->first;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_nearest_checkpoint_height(uint64_t block_height) const
  {
    if (m_points.empty())
      return 0;

    auto it = m_points.lower_bound(block_height);
    if (it == m_points.begin())
      return 0;

    --it;
    return it->first;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, crypto::hash>& checkpoints::get_points() const
  {
    return m_points;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, difficulty_type>& checkpoints::get_difficulty_points() const
  {
    return m_difficulty_points;
  }

  bool checkpoints::check_for_conflicts(const checkpoints& other) const
  {
    for (auto& pt : other.get_points())
    {
      if (m_points.count(pt.first))
      {
        CHECK_AND_ASSERT_MES(pt.second == m_points.at(pt.first), false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
      }
    }
    return true;
  }

  bool checkpoints::init_default_checkpoints(network_type nettype)
  {
    if (nettype == TESTNET)
    {
      return true;
    }
    if (nettype == STAGENET)
    {
      return true;
    }
    ADD_CHECKPOINT2(1,       "97f4ce4d7879b3bea54dcec738cd2ebb7952b4e9bb9743262310cd5fec749340", "0x2");
    ADD_CHECKPOINT2(6969,    "aa7b66e8c461065139b55c29538a39c33ceda93e587f84d490ed573d80511c87", "0x118eef693fd");  //Hard fork to v8
    ADD_CHECKPOINT2(53666,   "3f43f56f66ef0c43cf2fd14d0d28fa2aae0ef8f40716773511345750770f1255", "0xb677d6405ae");  //Hard fork to v9
    ADD_CHECKPOINT2(63469,   "4e33a9343fc5b86661ec0affaeb5b5a065290602c02d817337e4a979fe5747d8", "0xe7cd9819062");  //Hard fork to v10
    ADD_CHECKPOINT2(81769,   "41db9fef8d0ccfa78b570ee9525d4f55de77b510c3ae4b08a1d51b9aec9ade1d", "0x150066455b88"); //Hard fork to v11
    ADD_CHECKPOINT2(82069,   "fdea800d23d0b2eea19dec8af31e453e883e8315c97e25c8bb3e88ca164f8369", "0x15079b5fdaa8"); //Hard fork to v12
    ADD_CHECKPOINT2(114969,  "b48245956b87f243048fd61021f4b3e5443e57eee7ff8ba4762d18926e80b80c", "0x1ca552b3ec68"); //Hard fork to v13
    ADD_CHECKPOINT2(115257,  "338e056551087fe23d6c4b4280244bc5362b004716d85ec799a775f190f9fea9", "0x1cb25f5d4628"); //Hard fork to v14
    ADD_CHECKPOINT2(160777,  "9496690579af21f38f00e67e11c2e85a15912fe4f412aad33d1162be1579e755", "0x5376eaa196a8"); //Hard fork to v15
    ADD_CHECKPOINT2(253999,  "755a289fe8a68e96a0f69069ba4007b676ec87dce2e47dfb9647fe5691f49883", "0x172d026ef7fe8"); //Hard fork to v16
    ADD_CHECKPOINT2(254287,  "b37cb55abe73965b424f8028bf71bef98d069645077ffa52f0c134907b7734e3", "0x1746622f56668"); //Hard fork to v17
    ADD_CHECKPOINT2(256700,  "389a8ab95a80e84ec74639c1078bc67b33af208ef00f53bd9609cfc40efa7059", "0x185ace3c1bd68");
    ADD_CHECKPOINT2(271600,  "9597cdbdc52ca57d7dbd8f9c0a23a73194ef2ebbcfdc75c21992672706108d43", "0x1e2d2d6a2a9e8");
    ADD_CHECKPOINT2(278300,  "b10dcdf7a51651f60fbcc0447409773eef1458d2c706d9a61daf467571ac19c9", "0x20a83a16d3968");
    ADD_CHECKPOINT2(282700,  "79c06cafd7cb5f76bcebbf8f1ae16203bb41fd75b284bcd0eb0b457991ab7d4a", "0x22e3baf142de8");
    ADD_CHECKPOINT2(307686,  "dfd056b2739c132a07629409a59a028cb7414fac23e3419e79d2f49d66fc3af5", "0x305ba542e3ea8"); // "difficulty": 25800000
    ADD_CHECKPOINT2(307692,  "d822cd72037f62824ec87c9dc11768b45dc2632f697fa372e1885789c90f37fc", "0x305e124633878"); // "difficulty": 1890000
    ADD_CHECKPOINT2(307735,  "60970378aecdc0a78ccf5154edcc56f23aad8554b49e4716f820461a7588bfdc", "0x3070771b9ba58"); // "difficulty": 17900000
    ADD_CHECKPOINT2(307742,  "0ed835bc9fcd949b5a184cf607dcc62ac4268c9e4cf220f8b09bcce58f10916b", "0x30732f1248978"); // "difficulty": 21300000
    ADD_CHECKPOINT2(307750,  "7bcafbc757237125b70f569b181eb1b66c530b10d817d7b940f7a73dc827211c", "0x30766666b3d98"); // "difficulty": 10900000
    ADD_CHECKPOINT2(307766,  "02fd6c7d6bae710cfa3efb08f50e4bc9a590f6ab61eabd87e5e951338c0c36f6", "0x307d2d47a7918"); // "difficulty": 2960000
    ADD_CHECKPOINT2(307800,  "3594894b4231cfdfe911afed6552f9fb4cfe6048bacd0973a3a98623ec8548ce", "0x308b305ca7618");
    ADD_CHECKPOINT2(307880,  "659274b698f680c6cae2716cbd4e15ad5def23b5de98e53734c4af2c2e74bb7a", "0x30af6e91e8018");
    ADD_CHECKPOINT2(307883,  "9a8c35cd10963a14bba8a9628d1776df92fee5e3153b7249f5d15726efafaaea", "0x30b0965ba5a18");
    ADD_CHECKPOINT2(312130,  "e0da085bd273fff9f5f8e604fce0e91908bc62b6b004731a93e16e89cb9b1f54", "0x3cfe7148f2e18");
    ADD_CHECKPOINT2(324600,  "b24cd1ed7c192bbcf3d5b15729f2b032566687f96bda6f8cb73a5b16df4c6e6b", "0x69caecbe78718");
    ADD_CHECKPOINT2(327700,  "f113c8cbe077aab9296ecbfb41780c147aeb54edfece7e4b9946b8abd0f06de7", "0x732431429c818");
    ADD_CHECKPOINT2(331170,  "05243fba853fe375c671a6783eecac28777bca51f5977d5285c235424e52bb69", "0x7c3469310d218"); //Hard fork to v18
    ADD_CHECKPOINT2(331458,  "f79a664a5e4bc11fa7d804be2c3c72db50c87a27f1f540f337564cbb6314e4cd", "0x7c34d47adf218"); //Hard fork to v19
    ADD_CHECKPOINT2(331891,  "faceea4b4ab33fc962c24dfa2f98c2aeda4788f67c1e0044c62419912c1a64fe", "0x7c359086aeb58"); // restart DIFFICULTY_WINDOW
    ADD_CHECKPOINT2(332100,  "d32c409058c1eceb9a105190c7a5f480b2d6f49f318b18652b49ae971c710124", "0x7c538441cca36");
    ADD_CHECKPOINT2(334000,  "17d3b15f8e1a73e1c61335ee7979e9e3d211b9055e8a7fb2481e5f49a51b1c22", "0x7ddd5a79d69c4");
    ADD_CHECKPOINT2(348500,  "2d43a157f369e2aa26a329b56456142ecd1361f5808c688d97112a2e3bbd23f4", "0x90889ed877ada");
    ADD_CHECKPOINT2(489400,  "b14f49eae77398117ea93435676100d8b655a804689f73a5a4d0d5e71160d603", "0x1123c39bb52f7e");
    ADD_CHECKPOINT2(491200,  "cedba73ad35ce7f51aaca2beb36dc32d79ecc716d146eb8211e6a815f3666c4a", "0x11334734abbd17");
    ADD_CHECKPOINT2(497100,  "2c4c70ac1ada94151f19d67ccf1aa4e846e6067f49f67c85cc03f78e768ea42b", "0x116906bc97a751");
    ADD_CHECKPOINT2(760300,  "50ce41518bb4bea392194c13d0a5ef4cbf01ffb84ba393131e910adb63e2d360", "0x18ef58d8abb8b3");
    ADD_CHECKPOINT2(771100,  "03e834788e1e33dbba9bc3431a81189cd655f9da80323a728fa0dae56a95145e", "0x192cdb615ada62");
    ADD_CHECKPOINT2(838800,  "85ee72059e12e10a574628cac6f8ebcf8cf6cc1624a274c4ead39ede548777cd", "0x19f2d01d49339c");
    return true;
  }

  bool checkpoints::load_checkpoints_from_json(const std::string &json_hashfile_fullpath)
  {
    boost::system::error_code errcode;
    if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    LOG_PRINT_L1("Adding checkpoints from blockchain hashfile");

    uint64_t prev_max_height = get_max_height();
    LOG_PRINT_L1("Hard-coded max checkpoint height is " << prev_max_height);
    t_hash_json hashes;
    if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }
    for (std::vector<t_hashline>::const_iterator it = hashes.hashlines.begin(); it != hashes.hashlines.end(); )
    {
      uint64_t height;
      height = it->height;
      if (height <= prev_max_height) {
	LOG_PRINT_L1("ignoring checkpoint height " << height);
      } else {
	std::string blockhash = it->hash;
	LOG_PRINT_L1("Adding checkpoint height " << height << ", hash=" << blockhash);
	ADD_CHECKPOINT(height, blockhash);
      }
      ++it;
    }

    return true;
  }

  bool checkpoints::load_checkpoints_from_dns(network_type nettype)
  {
    std::vector<std::string> records;

    // All four MoneroPulse domains have DNSSEC on and valid
    static const std::vector<std::string> dns_urls = {
    };

    static const std::vector<std::string> testnet_dns_urls = {
    };

    static const std::vector<std::string> stagenet_dns_urls = {
    };

    if (!tools::dns_utils::load_txt_records_from_dns(records, nettype == TESTNET ? testnet_dns_urls : nettype == STAGENET ? stagenet_dns_urls : dns_urls))
      return true; // why true ?

    for (const auto& record : records)
    {
      auto pos = record.find(":");
      if (pos != std::string::npos)
      {
        uint64_t height;
        crypto::hash hash;

        // parse the first part as uint64_t,
        // if this fails move on to the next record
        std::stringstream ss(record.substr(0, pos));
        if (!(ss >> height))
        {
    continue;
        }

        // parse the second part as crypto::hash,
        // if this fails move on to the next record
        std::string hashStr = record.substr(pos + 1);
        if (!epee::string_tools::hex_to_pod(hashStr, hash))
        {
    continue;
        }

        ADD_CHECKPOINT(height, hashStr);
      }
    }
    return true;
  }

  bool checkpoints::load_new_checkpoints(const std::string &json_hashfile_fullpath, network_type nettype, bool dns)
  {
    bool result;

    result = load_checkpoints_from_json(json_hashfile_fullpath);
    if (dns)
    {
      result &= load_checkpoints_from_dns(nettype);
    }

    return result;
  }
}
