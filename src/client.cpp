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
  put(std::string nameStr)
  {
    // Nameify
    ndn::Name namePrefix(nameStr);
    populateStore(namePrefix, std::cin);

    // Register prefix and interest filter
    m_face.setInterestFilter(namePrefix, [this, namePrefix] (const auto&, const auto& interest) {
      std::shared_ptr<ndn::Data> data;
      const ndn::Name& iname = interest.getName();

      // Try to find this packet
      if (iname.size() == namePrefix.size() + 1 && iname[-1].isSegment()) {
        const auto segmentNo = static_cast<size_t>(iname[-1].toSegment());
        // specific segment retrieval
        if (segmentNo < m_store.size()) {
          data = m_store[segmentNo];
        }
      }
      else if (interest.matchesData(*m_store[0])) {
        // unspecified version or segment number, return first segment
        data = m_store[0];
      }

      if (data != nullptr) {
        m_face.put(*data);
      }
    }, std::bind(&Client::insertStore, this), nullptr); // Send INSERT command on registration

    m_face.processEvents();
  }

  void
  get(std::string nameStr)
  {
    ndn::Name namePrefix(nameStr);
    ndn::Name name(nameStr);
    name.appendSegment(0);
    pointer++;

    sendFETCH(name);

    m_face.processEvents();
  }

private:
  void
  sendFETCH(ndn::Name interestName)
  {
    pending++;

    ndn::Name hint("/kua");
    hint.appendNumber(hashFunc(interestName) % NUM_BUCKETS);
    hint.append("FETCH");

    ndn::Interest interest(interestName);
    interest.setMustBeFresh(false);
    interest.setCanBePrefix(false);
    interest.setForwardingHint(ndn::DelegationList({{15893, hint }}));

    m_face.expressInterest(interest,
                          [this, interestName] (const ndn::Interest&, const ndn::Data& data) {
                            std::cerr << "FETCH_SUCCESS=" << interestName << std::endl;
                            pending--;
                            done++;

                            if (data.getFinalBlock().has_value()) {
                              const auto sz = data.getFinalBlock().value().toSegment();
                              if (sz+1 != m_store.size())
                              {
                                m_store.resize(sz+1);
                              }
                            }
                            else
                            {
                              return;
                            }

                            if (data.getName()[-1].isSegment()) {
                              const auto segmentNo = static_cast<size_t>(data.getName()[-1].toSegment());
                              m_store[segmentNo] = std::make_shared<ndn::Data>(data);;
                            }

                            while (pointer < m_store.size() && pending < windowSize) {
                              ndn::Name namePrefix(interestName);
                              sendFETCH(namePrefix.getPrefix(-1).appendSegment(pointer));
                              pointer++;
                            }

                            if (done == m_store.size())
                            {
                              std::cerr << "FETCHED ALL SEGMENTS" << std::endl;
                              for (const auto& dptr : m_store)
                              {
                                const auto content = dptr->getContent();
                                std::cout.write(reinterpret_cast<const char*>(content.value()), content.value_size());
                              }
                            }
                          },
                          [this, interestName] (const ndn::Interest&, const ndn::lp::Nack& nack) {
                            std::cerr << "FETCH_NACK=" << interestName << std::endl;
                            pending--;
                          },
                          [this, interestName] (const ndn::Interest& interest) {
                            pending--;
                            std::cerr << "FETCH_RETRY=" << interestName << std::endl;
                            this->sendFETCH(interestName);
                          });
  }

  void
  insertStore()
  {
    while (pointer < m_store.size() && pending < windowSize) {
      sendINSERT(m_store[pointer]->getName());
      pointer++;
    }
  }

  void
  sendINSERT(const ndn::Name name)
  {
    pending++;

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
    m_keyChain.sign(interest, interestSigningInfo);

    // Send Interest
    std::cerr << "INSERT=" << name << " CMD=" << interestName << std::endl;
    m_face.expressInterest(interest,
                           [this, name, interestName] (const ndn::Interest&, const ndn::Data& data) {
                             std::cerr << "INSERT_SUCCESS=" << name << " CMD=" << interestName << std::endl;
                             pending--;
                             this->insertStore();
                           },
                           [this, name, interestName] (const ndn::Interest&, const ndn::lp::Nack& nack) {
                             std::cerr << "INSERT_NACK=" << name << " CMD=" << interestName << std::endl;
                             pending--;
                           },
                           [this, name, interestName] (const ndn::Interest& interest) {
                            pending--;
                            std::cerr << "INSERT_RETRY=" << name << " CMD=" << interestName << std::endl;
                            this->sendINSERT(name);
                           });
  }

  void
  populateStore(const ndn::Name name, std::istream& is)
  {
    std::vector<uint8_t> buffer(8000);
    while (is.good()) {
      is.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
      const auto nCharsRead = is.gcount();

      if (nCharsRead > 0) {
        auto data = std::make_shared<ndn::Data>(ndn::Name(name).appendSegment(m_store.size()));
        data->setFreshnessPeriod(ndn::time::seconds(10));
        data->setContent(buffer.data(), static_cast<size_t>(nCharsRead));
        m_store.push_back(data);
      }
    }

    if (m_store.empty()) {
      auto data = std::make_shared<ndn::Data>(ndn::Name(name).appendSegment(0));
      data->setFreshnessPeriod(ndn::time::seconds(10));
      m_store.push_back(data);
    }

    auto finalBlockId = ndn::name::Component::fromSegment(m_store.size() - 1);
    for (const auto& data : m_store) {
      data->setFinalBlock(finalBlockId);
      m_keyChain.sign(*data);
    }

    std::cerr << "Created " << m_store.size() << " chunks for prefix " << name << "\n";
  }

  void
  onData(const ndn::Interest&, const ndn::Data& data) const
  {
    std::cerr << "Received Data " << data << std::endl;
  }

  void
  onNack(const ndn::Interest&, const ndn::lp::Nack& nack) const
  {
    std::cerr << "Received Nack with reason " << nack.getReason() << std::endl;
  }

  void
  onTimeout(const ndn::Interest& interest) const
  {
    std::cerr << "Timeout for " << interest << std::endl;
  }

public:
  unsigned int windowSize = 20;

private:
  ndn::Face m_face;
  std::vector<std::shared_ptr<ndn::Data>> m_store;
  ndn::KeyChain m_keyChain;

  std::hash<ndn::Name> hashFunc;
  unsigned int pending = 0;
  unsigned int pointer = 0;
  unsigned int done = 0;
};

} // namespace kua

int
main(int argc, char** argv)
{
  try {
    kua::Client client;

    if (argc == 3 && std::string(argv[1]) == "get") {
      client.get(argv[2]);
    } else if (argc == 3 && std::string(argv[1]) == "put") {
      client.put(argv[2]);
    }
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
