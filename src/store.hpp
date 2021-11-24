#pragma once

#include <ndn-cxx/data.hpp>
#include "bucket.hpp"

namespace kua {

class Store {
public:
  Store(bucket_id_t bucketId) {}

  virtual bool
  put(const ndn::Data& data) = 0;

  virtual std::shared_ptr<const ndn::Data>
  get(const ndn::Name& dataName) = 0;

  virtual ~Store() = default;
};

} // namespace kua