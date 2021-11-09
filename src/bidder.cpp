#include "bidder.hpp"
#include "worker.hpp"

#include <ndn-cxx/util/logger.hpp>

namespace kua {

NDN_LOG_INIT(kua.bidder);

Bidder::Bidder(ConfigBundle& configBundle, NodeWatcher& nodeWatcher)
  : m_configBundle(configBundle)
  , m_syncPrefix(ndn::Name(configBundle.kuaPrefix).append("sync").append("auction"))
  , m_nodePrefix(configBundle.nodePrefix)
  , m_face(configBundle.face)
  , m_scheduler(m_face.getIoService())
  , m_keyChain(configBundle.keyChain)
  , m_nodeWatcher(nodeWatcher)
  , m_rndBid(1, 100)
  , m_rng(ndn::random::getRandomNumberEngine())
{
  NDN_LOG_INFO("Constructing Bidder");

  // Master's bidder does nothing
  if (m_configBundle.isMaster)
  {
    return;
  }

  // Initialize SVS
  m_svs = std::make_unique<ndn::svs::SVSync>(
    m_syncPrefix, m_nodePrefix, m_face, std::bind(&Bidder::updateCallback, this, _1));

  initialize();
}

void
Bidder::initialize()
{
  NDN_LOG_DEBUG("Initializing Bidder");
}

void
Bidder::updateCallback(const std::vector<ndn::svs::MissingDataInfo>& missingInfo)
{
  for (const auto m : missingInfo) {
    if (m.nodeId != ndn::Name(MASTER_PREFIX)) {
      continue;
    }

    for (ndn::svs::SeqNo i = m.low; i <= m.high; i++) {
      m_svs->fetchData(m.nodeId, i, std::bind(&Bidder::processMasterMessage, this, _1), 10);
    }
  }
}

void
Bidder::processMasterMessage(const ndn::Data& data)
{
  AuctionMessage msg;
  msg.wireDecode(data.getContent().blockFromValue());

  switch (msg.messageType)
  {
    case AuctionMessage::Type::Auction:
    {
      NDN_LOG_DEBUG("RECV_AUCTION for #" << msg.bucketId << " AID " << msg.auctionId);
      placeBid(msg.bucketId, msg.auctionId);
      break;
    }

    case AuctionMessage::Type::Win:
    {
      if (msg.winner == m_nodePrefix)
      {
        NDN_LOG_INFO("Won auction for #" << msg.bucketId);

        AuctionMessage rmsg(AuctionMessage::Type::WinAck, msg.auctionId, msg.bucketId);
        m_svs->publishData(rmsg.wireEncode(), ndn::time::milliseconds(1000));

        if (!m_buckets.count(msg.bucketId))
          m_buckets[msg.bucketId] = std::make_shared<Bucket>(msg.bucketId);

        // Log all buckets served
        std::stringstream ss;
        for (const auto& bucket : m_buckets)
          ss << " #" << bucket.first;
        NDN_LOG_INFO("Should have workers for" << ss.str());
      }
      break;
    }

    case AuctionMessage::Type::AuctionEnd:
    {
      if (!m_buckets.count(msg.bucketId))
        return;

      m_buckets[msg.bucketId]->confirmedHosts.clear();

      for (const auto& w : msg.winnerList)
      {
        m_buckets[msg.bucketId]->confirmedHosts[w] = 1;
        NDN_LOG_DEBUG("Confirmed node for #" << msg.bucketId << " " << w);
      }

      // Start the worker if not running
      if (!m_buckets[msg.bucketId]->worker)
        m_buckets[msg.bucketId]->worker = std::make_shared<Worker>(m_configBundle, *m_buckets[msg.bucketId]);

      break;
    }

    default:
      return;
  }
}

void
Bidder::placeBid(bucket_id_t bucketId, auction_id_t auctionId)
{
  auto bidAmount = m_rndBid(m_rng);
  bidAmount += 10000.0 / (m_buckets.size() + 1);

  AuctionMessage msg(AuctionMessage::Type::Bid, auctionId, bucketId);
  msg.bidAmount = bidAmount;

  NDN_LOG_DEBUG("PLACE_BID for #" << bucketId << " AID " << auctionId << " $" << bidAmount);
  m_svs->publishData(msg.wireEncode(), ndn::time::milliseconds(1000));
}

} // namespace kua