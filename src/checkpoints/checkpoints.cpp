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

#include "include_base_utils.h"

using namespace epee;

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "include_base_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"

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
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::parse_tpod_from_hex_string(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    // return false if adding at a height we already have AND the hash is different
    if (m_points.count(height))
    {
      CHECK_AND_ASSERT_MES(h == m_points[height], false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    m_points[height] = h;
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
    std::map< uint64_t, crypto::hash >::const_iterator highest = 
        std::max_element( m_points.begin(), m_points.end(),
                         ( boost::bind(&std::map< uint64_t, crypto::hash >::value_type::first, _1) < 
                           boost::bind(&std::map< uint64_t, crypto::hash >::value_type::first, _2 ) ) );
    return highest->first;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, crypto::hash>& checkpoints::get_points() const
  {
    return m_points;
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
      ADD_CHECKPOINT(1,     "97f4ce4d7879b3bea54dcec738cd2ebb7952b4e9bb9743262310cd5fec749340");
      ADD_CHECKPOINT(10,    "305472c87ff86d8afb3ec42634828462b0ed3d929fc05fa1ae668c3bee04837a");
      ADD_CHECKPOINT(100,   "a92b9deae26e19322041cbc2f850fa905748ae1e5bf69b35ca90b247c5cbfc04");
      ADD_CHECKPOINT(1000,  "62921e13030b29264439cafaf8320cf8aa039ee6ba7ba29c72f11b50a079269a");
      ADD_CHECKPOINT(2000,  "b3e1d73e3d0243239481aa76cb075cf2428556f5dc4f2e30428ea2ba36693e97");
      ADD_CHECKPOINT(3000,  "83a6e1ab394e80b8442b7b70b0e4c3a9fa0143e0ca51a33e829537ef5dd1bf13");
      ADD_CHECKPOINT(4000,  "7c70722d8cb8106b4bec67e1790614cc6e98db7afd0843b96cdff6960a0e0073");
      ADD_CHECKPOINT(5000,  "331ee74008e174e5fd1956f64c52793961b321a1366f7c6f7d324e8265df34f6");
      ADD_CHECKPOINT(6969,  "aa7b66e8c461065139b55c29538a39c33ceda93e587f84d490ed573d80511c87"); //Hard fork to v8
      ADD_CHECKPOINT(7000,  "2711bd33b107f744ad8bf98c1acefa18658780079496bd2f3a36f2e20b261f8e");
      ADD_CHECKPOINT(7500,  "5975967c4624f13f058acafe7adf9355e03e8e802eeadc84ccb22ea588bc0762");
      ADD_CHECKPOINT(7900,  "d9bc18cb35feb6b26bc5a19bbdbf7c852d9cc02883acb5bbce2e87d8b2c86069");
      ADD_CHECKPOINT(10000,  "bc5bfbf1b26c8f976d1d792ece4c6a7e93064bec62b72f1d5beae74c3f273b3b");
      ADD_CHECKPOINT(20000,  "52cc7edcb49eb02f28a653b824089a726f4050eb210263ee6f4180d388a1e5cc");
      ADD_CHECKPOINT(30000,  "d22fde5dd240ade16d3250eb0aa5d1c16dc7cb51c20484e05eb274911032b3fa");
      ADD_CHECKPOINT(40000,  "aee0d642322542ba069cb1c58ab2acd3560f108d4682c3dc3cb15a54d442d91f");
      ADD_CHECKPOINT(50000,  "5286ac2a0f39b3aefcba363cd71f2760bd1e0d763cbc81026ebdc3f80a86541f");
      ADD_CHECKPOINT(53666,  "3f43f56f66ef0c43cf2fd14d0d28fa2aae0ef8f40716773511345750770f1255"); //Hard fork to v9
      ADD_CHECKPOINT(54500,  "8ed3078b389c2b44add007803d741b58d3fbed2e1ba4139bda702152d8773c9b");
      ADD_CHECKPOINT(55000,  "4b662ceccefc3247edb4d654dd610b8fb496e85b88a5de43cc2bdd28171b15ff");
      ADD_CHECKPOINT(57000,  "08a79f09f12bb5d230b63963356a760d51618e526cfc636047a6f3798217c177");
      ADD_CHECKPOINT(59000,  "180b51ee2c5fbcd4362eb7a29df9422481310dd77d10bccdf8930724c31e007e");
      ADD_CHECKPOINT(59900,  "18cc0653ef39cb304c68045dba5eb6b885f936281cd939dea04d0e6c9cd4ae2e");
      ADD_CHECKPOINT(60000,  "0f02aa57a63f79f63dafed9063abe228a37cb19f00430dc3168b8a8f4ae8016c");
      ADD_CHECKPOINT(61000,  "509aca8c54eb5fe44623768757b6e890ae39d512478c75f614cbff3d91809350");
      ADD_CHECKPOINT(62000,  "7fe91ad256c08dbd961e04738968be22fb481093fbfa7959bde7796ccceba0e2");
      ADD_CHECKPOINT(62150,  "1a7c75f8ebeda0e20eb5877181eafd7db0fc887e3fed43e0b27ab2e7bccafd10");
      ADD_CHECKPOINT(62269,  "4969555d60742afb93925fd96d83ac28f45e6e3c0e583c9fb3c92d9b2100d38f");
      ADD_CHECKPOINT(62405,  "4d0ae890cf9f875f231c7069508ad28dc429d14814b52db114dfab7519a27584");
      ADD_CHECKPOINT(62419,  "bd8bf5ac4c4fb07ab4d0d492bd1699def5c095ab6943ad3b63a89d1d8b1ce748");
      ADD_CHECKPOINT(62425,  "41a922dba6f3906871b2ccaf31ec9c91033470c503959093dae796deda8940ea");
      ADD_CHECKPOINT(62479,  "a2e8ff4205ba2980eb70921b0b21b5fc656ee273664ea94b860c68ca069b60dd");
      ADD_CHECKPOINT(62503,  "25fa115962988b4b8f8cfd22744a3e653b22ead8c8468e64caf334fc75a97d08");
      ADD_CHECKPOINT(62550,  "bde522a8a81c392c98c979434aa1dd9d20b4ca52230ba6ae0362872757808a48");
      ADD_CHECKPOINT(62629,  "8368e1ce1d421f1fc969364558433e2b2363d0ffcb5f2d946633095e3e6734f5");
      ADD_CHECKPOINT(62720,  "f871cddd75951e2fe24c282d2bd28396fc922ea519b354ace992a0162cb333ff");
      ADD_CHECKPOINT(62733,  "8331dbeeaf23173d2235a062373a437befadb6492cceb7640127bf18653a9e61");
      ADD_CHECKPOINT(62877,  "62d44adc05d7d4fd9d15239c5575612207beab0bcf2da49158bf89e365441ca1");
      ADD_CHECKPOINT(63469,  "4e33a9343fc5b86661ec0affaeb5b5a065290602c02d817337e4a979fe5747d8"); //Hard fork to v10
      ADD_CHECKPOINT(63950,  "155b61475985ac3f48fda10091d732bdc8087a55554504959e88d29962c91b72");


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
        if (!epee::string_tools::parse_tpod_from_hex_string(hashStr, hash))
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



