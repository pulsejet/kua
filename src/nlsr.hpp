#pragma once

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/mgmt/nfd/control-parameters.hpp>
#include <ndn-cxx/mgmt/nfd/control-response.hpp>
#include <ndn-cxx/security/interest-signer.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>

#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/random.hpp>

namespace kua {

class NLSR
{
public:
  NLSR(ndn::security::KeyChain& keyChain, ndn::Face& face);

  void
  advertise(const ndn::Name& name);

private:

  ndn::Interest
  advertiseInterest(const ndn::Name& name);

  ndn::Interest
  getNamePrefixInterst(const ndn::Name& name,
                      const ndn::Name::Component& verb);

private:
  ndn::random::RandomNumberEngine& m_rng;
  ndn::Face& m_face;
  ndn::KeyChain& m_keyChain;
  ndn::Scheduler m_scheduler;
  std::uniform_int_distribution<> m_jitter;
};

} // namespace kua
