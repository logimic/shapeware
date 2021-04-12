#pragma once

#include <vector>

namespace shape {
  std::vector<std::string> tokenizeTopic(const std::string & tokens, char delimit = '/')
  {
    std::vector<std::string> retval;
    std::string s = tokens;
    size_t pos = 0;

    while ((pos = s.find(delimit)) != std::string::npos) {
      retval.push_back(s.substr(0, pos));
      s.erase(0, pos + 1);
    }
    retval.push_back(s);

    return retval;
  }
}
