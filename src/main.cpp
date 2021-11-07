#include <iostream>

#include "config-bundle.hpp"
#include "node-watcher.hpp"
#include "bidder.hpp"

int
main(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "Usage: kua <kua-prefix> <node-prefix>" << std::endl;
    abort();
  }

  // Get arguments
  const ndn::Name kuaPrefix(argv[1]);
  const ndn::Name nodePrefix(argv[2]);

  // Start face and keychain
  ndn::Face face;
  ndn::KeyChain keyChain;

  // Create common bundle
  ConfigBundle configBundle { kuaPrefix, nodePrefix, face, keyChain };

  // Start components
  kua::NodeWatcher nodeWatcher(configBundle);
  kua::Bidder bidder(configBundle, nodeWatcher);

  // Infinite loop
  face.processEvents();
}