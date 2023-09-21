#ifndef STREAMREADER_H
#define STREAMREADER_H

#include <memory>
#include <string>

namespace jsonrpc {
  class StreamReader {
  public:
    StreamReader(size_t buffersize);
    virtual ~StreamReader();

    bool Read(std::string &target, int fd, char delimiter);

  private:
    size_t buffersize;
    char *buffer;
  };
} // namespace jsonrpc
#endif // STREAMREADER_H
