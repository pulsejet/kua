#include <iostream>
#include <string>

#include "config-bundle.hpp"
#include "node-watcher.hpp"
#include "bidder.hpp"
#include "master.hpp"

int
main(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "Usage: kua <kua-prefix> <node-prefix>" << std::endl;
    exit(1);
  }

  // Get arguments
  const ndn::Name kuaPrefix(argv[1]);
  const ndn::Name nodePrefix(argv[2]);

  const bool isMaster =
#ifdef KUA_IS_MASTER
    true;
#else
    false;
#endif

  // Start face and keychain
  ndn::Face face;
  ndn::KeyChain keyChain;

  // Create common bundle
  kua::ConfigBundle configBundle { kuaPrefix, nodePrefix, face, keyChain, isMaster };

  // Start components
  kua::NodeWatcher nodeWatcher(configBundle);
  kua::Bidder bidder(configBundle, nodeWatcher);

  std::unique_ptr<kua::Master> master;
  if (isMaster) {
    master = std::make_unique<kua::Master>(configBundle, nodeWatcher);
  }

  // Infinite loop
  face.processEvents();
}