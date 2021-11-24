#include <ndn-cxx/face.hpp>
#include <ndn-cxx/ims/in-memory-storage-persistent.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include <iostream>
#include "config-bundle.hpp"

namespace kua {

class Client
{
public:
  void
  put(std::string nameStr, std::string contents)
  {
    // Nameify
    ndn::Name name(nameStr);

    // Create all packets
    std::shared_ptr<ndn::Data> packet(std::make_shared<ndn::Data>(name));
    packet->setContent(reinterpret_cast<const uint8_t*>(contents.c_str()), contents.size());
    m_keychain.sign(*packet);
    m_ims.insert(*packet);

    // Register prefix and interest filter
    m_face.setInterestFilter(name, [this] (const auto&, const auto& interest) {
      // Try to find this packet
      auto dataptr = m_ims.find(interest);
      if (dataptr)
        m_face.put(*dataptr);
    }, std::bind(&Client::sendINSERT, this, name), nullptr); // Send INSERT command on registration

    m_face.processEvents();
  }

  void
  get(std::string nameStr)
  {
    ndn::Name name(nameStr);

    ndn::Name hint("/kua");
    hint.appendNumber(hashFunc(name) % NUM_BUCKETS);
    hint.append("FETCH");

    ndn::Interest interest(name);
    interest.setMustBeFresh(false);
    interest.setCanBePrefix(false);
    interest.setForwardingHint(ndn::DelegationList({{15893, hint }}));
    m_face.expressInterest(interest,
                           bind(&Client::onData, this,  _1, _2),
                           bind(&Client::onNack, this, _1, _2),
                           bind(&Client::onTimeout, this, _1));

    m_face.processEvents();
  }

private:
  void
  sendINSERT(const ndn::Name name)
  {
    // Make command
    ndn::Name interestName("/kua");
    interestName.appendNumber(hashFunc(name) % NUM_BUCKETS);
    interestName.append("INSERT");
    interestName.append(name.wireEncode());

    // Create interest
    ndn::Interest interest(interestName);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::milliseconds(3000));

    // Replicate to other nodes
    ndn::Name params;
    params.append("REPLICATE");
    interest.setApplicationParameters(params.wireEncode());

    // Sign
    ndn::security::SigningInfo interestSigningInfo;
    interestSigningInfo.setSignedInterestFormat(ndn::security::SignedInterestFormat::V03);
    m_keychain.sign(interest, interestSigningInfo);

    // Send Interest
    std::cout << "INSERT=" << name << " CMD=" << interestName << std::endl;
    m_face.expressInterest(interest,
                           [this, name, interestName] (const ndn::Interest&, const ndn::Data& data) {
                             std::cout << "INSERT_SUCCESS=" << name << " CMD=" << interestName << std::endl;
                           },
                           bind(&Client::onNack, this, _1, _2),
                           bind(&Client::onTimeout, this, _1));
  }


  void
  onData(const ndn::Interest&, const ndn::Data& data) const
  {
    std::cout << "Received Data " << data << std::endl;
  }

  void
  onNack(const ndn::Interest&, const ndn::lp::Nack& nack) const
  {
    std::cout << "Received Nack with reason " << nack.getReason() << std::endl;
  }

  void
  onTimeout(const ndn::Interest& interest) const
  {
    std::cout << "Timeout for " << interest << std::endl;
  }

private:
  ndn::Face m_face;
  ndn::InMemoryStoragePersistent m_ims;
  ndn::KeyChain m_keychain;

  std::hash<ndn::Name> hashFunc;
};

} // namespace kua

int
main(int argc, char** argv)
{
  try {
    kua::Client client;

    if (argc == 2) {
      client.get(argv[1]);
    } else if (argc == 3) {
      client.put(argv[1], argv[2]);
    }
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
