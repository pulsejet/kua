#pragma once

#include <ndn-cxx/name.hpp>
#include <map>

namespace kua {

struct Bucket
{
  unsigned int id;
  std::map<ndn::Name, int> pendingHosts;
  std::map<ndn::Name, int> confirmedHosts;
};

} // namespace kua