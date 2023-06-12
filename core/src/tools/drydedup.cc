#include <iostream>
#include <vector>
#include <cstdio>
#include <unordered_set>
#include <map>
#include <optional>

#include "lib/cli.h"
#include "lib/version.h"
#include <openssl/evp.h>

class openssl_digest
{
public:
  openssl_digest(char* digest, unsigned length) : digest{digest}
						, length{length}
  {
  }

  openssl_digest(const openssl_digest&) = delete;
  openssl_digest& operator=(const openssl_digest&) = delete;

  openssl_digest(openssl_digest&& to_move)
  {
    *this = std::move(to_move);
  }
  openssl_digest& operator=(openssl_digest&& to_move)
  {
    std::swap(digest, to_move.digest);
    std::swap(length, to_move.length);
    return *this;
  }

  const char* data() const {
    return digest;
  }

  std::size_t size() const {
    return length;
  }

  ~openssl_digest()
  {
    if (digest) { OPENSSL_free(digest); }
  }
private:
  char* digest{nullptr};
  unsigned length{0};
};

std::optional<openssl_digest> digest_message(const char *message,
					     size_t message_len)
{
	EVP_MD_CTX *mdctx;

	if((mdctx = EVP_MD_CTX_new()) == NULL) {
	  return std::nullopt;
	}

	if(1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
	  return std::nullopt;
	}

	if(1 != EVP_DigestUpdate(mdctx, message, message_len)) {
	  return std::nullopt;
	}

	char* digest = (char *)OPENSSL_malloc(EVP_MD_size(EVP_sha256()));
	unsigned length = 0;
	if(digest == NULL) {
	  return std::nullopt;
	}

	if(1 != EVP_DigestFinal_ex(mdctx, (unsigned char*)digest, &length)) {
	  OPENSSL_free(digest);
	  return std::nullopt;
	}

	EVP_MD_CTX_free(mdctx);

	return openssl_digest{ digest, length };
}


struct block {
  using it = std::vector<char>::const_iterator;

  block(std::size_t size, it begin, it end,
	char fill) : data(size, fill)
  {
    if (static_cast<std::size_t>(end - begin) <= size) {
      std::copy(begin, end, data.begin());
    } else {
      std::copy(begin, begin + size, data.begin());
    }
    std::optional digest = digest_message(&*data.begin(), data.size());
    if (!digest.has_value()) {
      throw;
    }
    checksum = std::string(digest->data(), digest->size());
  }

  friend bool operator==(const block& a, const block& b) {
    return b.checksum == a.checksum && b.data == a.data;
  }

  const std::string& hash() const { return checksum; }
private:
  std::vector<char> data;
  std::string checksum;
};

namespace std {
  template <>
  struct hash<block> {
    std::size_t operator()(const block& b) const {
      return std::hash<std::string>{}(b.hash());
    }
  };
};

std::size_t add_blocks(std::unordered_set<block>& uniques,
		       const std::vector<char>& data,
		       std::size_t blocksize,
		       std::size_t cutoff,
		       char fill = 0)
{
  auto current = data.begin();
  std::size_t num = 0;
  while (current != data.end()) {
    auto end = current + blocksize;
    auto real_end = std::min(data.end(), end);
    if (static_cast<decltype(cutoff)>(real_end - current) < cutoff) { break; }
    uniques.emplace(blocksize, current, real_end, fill);
    num += 1;
    current = real_end;
  }

  return num;
}

// wir wollen sowas wie
// cutoff : x      große vorher  große danach  blocks vorher blocks danach
// cutoff : y      ....
// cutoff : z      ....
// cutoffs sollten sich hierbei aus der blocksize berechnen
// oder alternative sollten sie vom user als liste angegeben werden.
// zb. cutoff = blocksize, blocksize/2, blocksize/4, ...
// oder cutoff = 100% * blocksize, 75% * blocksize, 50% * blocksize, etc.
// probably does not work correctly since we want to use this with just
// the volume not with the actual file tree
// vielleicht ein zweites Tool, dass im filetree guckt, wie viele Blöcke es
// gibt, die größer sind als der cutoff + wie viele Daten das sind
// + was ein %satz das ist.

int main(int argc, char* argv[])
{

  CLI::App app;
  std::string desc(1024, '\0');
  kBareosVersionStrings.FormatCopyright(desc.data(), desc.size(), 2023);
  desc += "The Bareos DryDedup Tool.";
  InitCLIApp(app, desc, 0);

  std::size_t blocksize = 1 << 16;
  std::size_t cutoff    = 0;
  std::vector<std::string> files;
  std::string file_list;
  std::map<std::string, std::size_t> unit_map = {{"k", 1024}};
  app.add_option("-b,--blocksize", blocksize)
    ->transform(CLI::AsNumberWithUnit(unit_map));
  app.add_option("-c,--cut-off", cutoff)
    ->transform(CLI::AsNumberWithUnit(unit_map));
  auto* group = app.add_option_group("file input", "some description")
    ->required();
  group->add_option("-f,--files,files", files);
  group->add_option("-l,--file-list", file_list);

  CLI11_PARSE(app, argc, argv);

  if (cutoff > blocksize) {
    std::cerr << "Cutoff needs to be at most blocksize.\n";
    return 1;
  }

  if (file_list.size() != 0) {
    std::ifstream fl(file_list.c_str());

    if (!fl) {
      std::cerr << "Could not open filelist '" << file_list << "'\n";
    } else {
      for (std::string line; std::getline(fl, line);) {
	files.emplace_back(std::move(line));
      }
    }
  }

  std::unordered_set<block> unique_blocks;
  std::size_t num_total_blocks{0};
  std::vector<char> data;

  for (auto& file : files) {
    auto* f = std::fopen(file.c_str(), "rb");
    if (!f) {
      std::cerr << "Could not open file '" << file << "'\n";
      continue;
    }
    if (std::fseek(f, 0, SEEK_END) != 0) {
      std::cerr << "Error while processing file '" << file << "'\n";
      std::fclose(f);
      continue;
    }
    auto size = std::ftell(f);
    if (std::fseek(f, 0, SEEK_SET) != 0) {
      std::cerr << "Error while processing file '" << file << "'\n";
      std::fclose(f);
      continue;
    }

    data.resize(size);
    auto num_read = std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);

    if (num_read != static_cast<decltype(num_read)>(size)) {
      std::cerr << num_read << " != " << size << std::endl;
      continue;
    }

    num_total_blocks += add_blocks(unique_blocks, data, blocksize, cutoff);
    data.clear();
  }

  if (num_total_blocks == 0) {
    std::cerr << "No files were processed." << std::endl;
    return 1;
  }

  std::cout << "From " << num_total_blocks << " to "
	    << unique_blocks.size() << std::endl;

  return 0;
}
