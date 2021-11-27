#include <ndn-cxx/face.hpp>
#include <ndn-cxx/ims/in-memory-storage-persistent.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include <iostream>
#include <chrono>

#include "config-bundle.hpp"
#include "bucket.hpp"
#include "command-codes.hpp"
#include "nlsr.hpp"

// #define VEROBSE

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

    auto nlsr = std::make_shared<NLSR>(m_keyChain, m_face);

    // Register prefix and interest filter
    m_face.setInterestFilter(namePrefix, [this, namePrefix, nlsr] (const auto&, const auto& interest) {
      // Advertise to NLSR
      nlsr->advertise(namePrefix);

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
    end_time = std::chrono::high_resolution_clock::now();
    print_time();
  }

  void
  get(std::string nameStr)
  {
    ndn::Name namePrefix(nameStr);
    ndn::Name name(nameStr);
    name.appendSegment(0);
    pointer++;

    sendFETCH(name);

    start_time = std::chrono::high_resolution_clock::now();
    m_face.processEvents();
    end_time = std::chrono::high_resolution_clock::now();
    print_time();
  }

  void
  print_time()
  {
    auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    uint64_t dataSize = 0;
    for (const auto& data : m_store)
      if (data)
        dataSize += data->getContent().size();

    std::cerr << "Processed " << dataSize / 1000 << "KB in " << ms_int.count() << "ms" << std::endl;
  }

private:
  void
  sendFETCH(ndn::Name interestName)
  {
    pending++;

    // Get bucket ID
    const auto bucketId = Bucket::idFromName(interestName);

    ndn::Name hint("/kua");
    hint.appendNumber(bucketId);
    hint.appendNumber(CommandCodes::FETCH);

    ndn::Interest interest(interestName);
    interest.setMustBeFresh(false);
    interest.setCanBePrefix(false);
    interest.setForwardingHint(ndn::DelegationList({{15893, hint }}));

    m_face.expressInterest(interest,
                          [this, interestName] (const ndn::Interest&, const ndn::Data& data) {
#ifdef VERBOSE
                            std::cerr << "FETCH_SUCCESS=" << interestName << std::endl;
#endif
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
    if (!pointer)
      start_time = std::chrono::high_resolution_clock::now();

    while (pointer < m_store.size() && pending < windowSize) {
      sendINSERT();
    }
  }

  void
  sendINSERT()
  {
    // Start segment
    const auto firstName = m_store[pointer]->getName();

    // Range end segment
    auto endSeg = firstName[-1].toSegment() - 1;

    // Get bucket ID
    const auto bucketId = Bucket::idFromName(firstName);

    while (pointer < m_store.size() && Bucket::idFromName(m_store[pointer]->getName()) == bucketId)
    {
      pointer++;
      pending++;
      endSeg++;
    }

    // Make command
    ndn::Name interestName("/kua");
    interestName.appendNumber(bucketId);
    interestName.append(ndn::Name(firstName).appendSegment(endSeg).wireEncode());
    interestName.appendNumber(CommandCodes::INSERT | CommandCodes::IS_RANGE);

    // Create interest
    ndn::Interest interest(interestName);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);
    interest.setInterestLifetime(ndn::time::milliseconds(3000));

    sendRangeInsert(interest);
  }

  void
  sendRangeInsert(const ndn::Interest& interest)
  {
    const ndn::Name interestName(interest.isSigned() ? interest.getName().getPrefix(-1) : interest.getName());
    const ndn::Name dataName(interestName[-2].blockFromValue());
    const uint64_t endSeg = dataName[-1].toSegment();
    const uint64_t startSeg = dataName[-2].toSegment();
    const uint64_t count = endSeg - startSeg + 1;

    // Refresh and sign
    ndn::Interest newInterest(interestName);
    ndn::security::SigningInfo interestSigningInfo;
    interestSigningInfo.setSignedInterestFormat(ndn::security::SignedInterestFormat::V03);
    m_keyChain.sign(newInterest, interestSigningInfo);

    // Send Interest
    m_face.expressInterest(newInterest,
                           [this, count, startSeg, endSeg] (const ndn::Interest& interest, const ndn::Data& data) {
                              // std::cerr << "INSERT_SUCCESS RANGE=" << startSeg << "-->" << endSeg << std::endl;
                              pending -= count;
                              done += count;

                              if (done == m_store.size())
                                return m_face.shutdown();

                             this->insertStore();
                           },
                           [this, count] (const ndn::Interest& interest, const ndn::lp::Nack& nack) {
                              std::cerr << "INSERT_NACK CMD=" << interest.getName() << std::endl;
                              pending -= count;
                           },
                           [this] (const ndn::Interest& interest) {
                              std::cerr << "INSERT_RETRY CMD=" << interest.getName() << std::endl;
                              this->sendRangeInsert(interest);
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
  unsigned int windowSize = 300;

private:
  ndn::Face m_face;
  std::vector<std::shared_ptr<ndn::Data>> m_store;
  ndn::KeyChain m_keyChain;

  unsigned int pending = 0;
  unsigned int pointer = 0;
  unsigned int done = 0;

  std::chrono::_V2::system_clock::time_point start_time;
  std::chrono::_V2::system_clock::time_point end_time;
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
