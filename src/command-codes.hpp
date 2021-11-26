#pragma once

namespace kua {

enum CommandCodes
{
  INSERT          = 0b00000001,
  NO_REPLICATE    = 0b00000010,
  IS_RANGE        = 0b00000100,
  FETCH           = 0b10000000,
};

} // namespace kua