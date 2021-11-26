#pragma once

#include "config-bundle.hpp"

#include <ndn-cxx/name.hpp>

#include <map>

namespace kua {

typedef unsigned int bucket_id_t;

class Worker;

class Bucket
{
public:
  Bucket() = delete;
  Bucket(bucket_id_t _id) : id(_id) {}

  bucket_id_t id;
  std::map<ndn::Name, int> pendingHosts;
  std::map<ndn::Name, int> confirmedHosts;
  std::shared_ptr<Worker> worker;

  static inline bucket_id_t
  idFromName(const ndn::Name& origName)
  {
    ndn::Name name(origName);

    if (name.size() >= 1 && name[-1].isSegment()) {
      uint64_t seg = name[-1].toSegment();
      name.erase(-1);
      name.appendSegment(seg / 100);
    }

    static std::hash<ndn::Name> hashFunc;
    return hashFunc(name) % NUM_BUCKETS;
  }
};

} // namespace kua