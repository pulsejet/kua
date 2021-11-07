#pragma once

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/face.hpp>

struct ConfigBundle
{
  const ndn::Name kuaPrefix;
  const ndn::Name nodePrefix;
  ndn::Face& face;
  ndn::KeyChain& keyChain;
  const bool isMaster;
};
