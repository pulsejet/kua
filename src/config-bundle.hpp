#pragma once

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/face.hpp>

struct ConfigBundle
{
  ndn::Name kuaPrefix;
  ndn::Name nodePrefix;
  ndn::Face& face;
  ndn::KeyChain& keyChain;
};
