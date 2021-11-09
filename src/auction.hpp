#pragma once

#include "tlv.hpp"
#include "bucket.hpp"

#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/name.hpp>

typedef unsigned int auction_id_t;

namespace kua {

struct AuctionMessage
{
  enum Type {
    Auction = 1,
    Bid = 2,
    Win = 3,
    WinAck = 4,
    AuctionEnd = 5,
  };

  Type messageType;

  auction_id_t auctionId;
  bucket_id_t bucketId;

  AuctionMessage() = default;

  AuctionMessage(Type _messageType,
                 auction_id_t _auctionId,
                 bucket_id_t _bucketId);

  // TypeBid
  unsigned int bidAmount;

  // TypeWin and TypeWinAck
  ndn::Name winner;

  // TypeAuctionEnd
  std::vector<ndn::Name> winnerList;

  void
  wireDecode(const ndn::Block& block);

  ndn::Block
  wireEncode() const;
};

} // namespace kua