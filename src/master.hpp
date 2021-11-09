#pragma once

#include <ndn-svs/svsync.hpp>

#include "config-bundle.hpp"
#include "node-watcher.hpp"
#include "bucket.hpp"
#include "auction.hpp"

namespace kua {

class Master
{
public:
  /** Initialize the bidder with the sync prefix */
  Master(ConfigBundle& configBundle, NodeWatcher& nodeWatcher);
private:
  struct Bid
  {
    ndn::Name bidder;
    unsigned int amount;

    bool operator< (const Bid& rhs) const {
        return amount < rhs.amount;
    }
  };

private:
  /**
   * Attempt to initialize master
   * If there are less than 3 nodes known, init will retry later
   */
  void
  initialize();

  /** On SVS update */
  void
  updateCallback(const std::vector<ndn::svs::MissingDataInfo>& missingInfo);

  /** Start a new auction */
  void
  auction();

  /** Start a new auction for a given bucket */
  void
  auction(unsigned int id);

  /** Process packet from master */
  void
  processMessage(const ndn::Name& sender, const ndn::Data& data);

  /** End an auction and generate the winners reply */
  void
  declareAuctionWinners();

  /** End the auction with the final message */
  void
  endAuction();

  /** Create a new auction message */
  inline AuctionMessage
  newMsg(AuctionMessage::Type type)
  {
    return AuctionMessage(type, m_currentAuctionId, m_currentAuctionBucketId);
  }

private:
  ConfigBundle& m_configBundle;
  ndn::Name m_syncPrefix;
  ndn::Name m_nodePrefix;
  ndn::Face& m_face;
  ndn::Scheduler m_scheduler;
  ndn::KeyChain& m_keyChain;
  NodeWatcher& m_nodeWatcher;

  ndn::random::RandomNumberEngine& m_rng;

  bool m_initialized = false;

  /** List of buckets */
  std::vector<Bucket> m_buckets;

  /** Periodic check for auctions */
  ndn::scheduler::ScopedEventId m_auctionRecheckEvent;

  /** Identifier for current auction. 0 if no auction. */
  auction_id_t m_currentAuctionId = 0;
  /** Bucket ID for the auction that is currently happening */
  bucket_id_t m_currentAuctionBucketId = 0;
  /** Expected number of bids for current auction */
  unsigned int m_currentAuctionNumBidsExpected = 0;
  /** Time for which the auction is running */
  unsigned int m_currentAuctionTime = 0;
  /** Bids for this bucket */
  std::vector<Bid> m_currentAuctionBids;

  std::unique_ptr<ndn::svs::SVSync> m_svs;
};

} // namespace kua