#pragma once

#include <ndn-cxx/name.hpp>
#include <map>

namespace kua {

typedef unsigned int bucket_id_t;

class Worker;

struct Bucket
{
  Bucket() = delete;
  Bucket(bucket_id_t _id) : id(_id) {}

  bucket_id_t id;
  std::map<ndn::Name, int> pendingHosts;
  std::map<ndn::Name, int> confirmedHosts;
  std::shared_ptr<Worker> worker;
};

} // namespace kua