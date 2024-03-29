#pragma once

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/face.hpp>

namespace kua {

struct ConfigBundle
{
  const ndn::Name kuaPrefix;
  const ndn::Name nodePrefix;
  ndn::Face& face;
  ndn::KeyChain& keyChain;
  const bool isMaster;
};

} // namespace kua

// Config options that need to go into config file
#define MASTER_PREFIX "/master"
#define NUM_BUCKETS 16
#define NUM_REPLICA 3
#define AUCTION_TIME_LIMIT 5
