#include <ndn-cxx/face.hpp>
#include <ndn-cxx/ims/in-memory-storage-persistent.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include <iostream>


namespace kua {

class Client
{
public:
  void
  run(std::string name, std::string contents)
  {
    std::shared_ptr<ndn::Data> packet(std::make_shared<ndn::Data>(ndn::Name(name)));
    packet->setContent(reinterpret_cast<const uint8_t*>(contents.c_str()), contents.size());
    m_keychain.sign(*packet);
    m_ims.insert(*packet);
    std::cout << "Created packet " << name << std::endl;

    m_face.setInterestFilter(ndn::Name(name), [this] (const auto&, const auto& interest) {
      // Try to find this packet
      std::cout << "GOT INTEREST" << interest << std::endl;
      auto dataptr = m_ims.find(interest);
      if (dataptr) {
        std::cout << *dataptr << std::endl;
        m_face.put(*dataptr);
      }
    }, std::bind(&Client::sendRequests, this), nullptr); // Send command on registration

    m_face.processEvents();
  }

  void
  sendRequests()
  {
    std::cout << "sending request" << std::endl;

    ndn::Name interestName("/kua");
    interestName.appendNumber(3);
    interestName.append("INSERT");
    interestName.append(ndn::Name("/ndn/test").wireEncode());

    ndn::Interest interest(interestName);

    // interest.setForwardingHint(ndn::DelegationList({{15893, "example/testApp/randomData"}}));

    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::milliseconds(3000)); // The default is 4 seconds

    ndn::Name params;
    params.append("REPLICATE");
    interest.setApplicationParameters(params.wireEncode());

    ndn::security::SigningInfo interestSigningInfo;
    interestSigningInfo.setSignedInterestFormat(ndn::security::SignedInterestFormat::V03);
    m_keychain.sign(interest, interestSigningInfo);

    std::cout << "Sending Interest " << interest << std::endl;
    m_face.expressInterest(interest,
                           bind(&Client::onData, this,  _1, _2),
                           bind(&Client::onNack, this, _1, _2),
                           bind(&Client::onTimeout, this, _1));
  }

private:
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
};

} // namespace kua

int
main(int argc, char** argv)
{
  try {
    kua::Client client;
    client.run(argv[1], argv[2]);
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
