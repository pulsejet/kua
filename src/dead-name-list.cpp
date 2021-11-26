#include "dead-name-list.hpp"

namespace kua {

bool
DeadNameList::has(const ndn::Name& name) const
{
  Entry entry = makeEntry(name);
  return m_ht.find(entry) != m_ht.end();
}

void
DeadNameList::add(const ndn::Name& name)
{
  Entry entry = makeEntry(name);
  if (m_ht.find(entry) != m_ht.end())
    return;

  m_queue.push_back(entry);

  // Evict old entries
  if (m_queue.size() > m_capacity)
  {
    auto nEvict = m_capacity / 2;
    for (size_t i = 0; i < nEvict; i++) {
      m_queue.pop_front();
    }
  }
}

DeadNameList::Entry
DeadNameList::makeEntry(const ndn::Name& name) const
{
  return hashFunc(name);
}

} // namespace kua
