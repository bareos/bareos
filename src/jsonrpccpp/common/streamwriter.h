#ifndef STREAMWRITER_H
#define STREAMWRITER_H

#include <memory>
#include <string>

namespace jsonrpc {
  class StreamWriter {
  public:
    bool Write(const std::string &source, int fd);
  };

} // namespace jsonrpc

#endif // STREAMWRITER_H
