#pragma once

#include "store.hpp"
#include <map>

namespace kua {

class StoreMemory : public Store
{
public:
  StoreMemory(bucket_id_t bucketId) : Store(bucketId) { }

  inline bool
  put(const ndn::Data& data)
  {
    std::shared_ptr<const ndn::Data> ptr = std::make_shared<const ndn::Data>(data);
    m_map[data.getName()] = ptr;
    return true;
  }

  inline std::shared_ptr<const ndn::Data>
  get(const ndn::Name& dataName)
  {
    if (m_map.find(dataName) != m_map.end())
    {
      return m_map.at(dataName);
    }
    else
    {
      return nullptr;
    }
  }

private:
  std::map<ndn::Name, std::shared_ptr<const ndn::Data>> m_map;
};

} // namespace kua