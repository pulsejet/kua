#pragma once

#include <ndn-cxx/name.hpp>
#include <map>

namespace kua {

class Worker;

struct Bucket
{
  unsigned int id;
  std::map<ndn::Name, int> pendingHosts;
  std::map<ndn::Name, int> confirmedHosts;
  std::shared_ptr<Worker> worker;
};

} // namespace kua