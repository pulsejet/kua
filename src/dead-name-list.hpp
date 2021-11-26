#pragma once

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/interest.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>

namespace kua {

class DeadNameList
{
public:
  DeadNameList() {}

  /**
   * \brief Determines if name+nonce is in the list
   * \return true if name+nonce exists, false otherwise
   */
  bool
  has(const ndn::Name& name) const;

  /**
   * \brief Adds name+nonce to the list
   */
  void
  add(const ndn::Name& name);

private:
  using Entry = size_t;

  Entry
  makeEntry(const ndn::Name& name) const;

  struct Queue {};
  struct Hashtable {};
  using Container = boost::multi_index_container<
    Entry,
    boost::multi_index::indexed_by<
      boost::multi_index::sequenced<boost::multi_index::tag<Queue>>,
      boost::multi_index::hashed_non_unique<boost::multi_index::tag<Hashtable>,
                                            boost::multi_index::identity<Entry>>
    >
  >;

  Container m_index;
  Container::index<Queue>::type& m_queue = m_index.get<Queue>();
  Container::index<Hashtable>::type& m_ht = m_index.get<Hashtable>();

  std::hash<ndn::Name> hashFunc;

  size_t m_capacity = 4096;
};

} // namespace kua
