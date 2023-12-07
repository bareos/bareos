/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "lib/tree_save.h"
#include "lib/xxhash.h"

#include <cstring>
#include <cstdint>

extern "C" {
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
}

template <typename T> struct span {
  const T* mem{nullptr};
  std::size_t count{0};

  span() {}
  span(const T* mem, std::size_t count) : mem{mem}, count{count} {}

  const T* begin() const { return mem; }
  const T* end() const { return mem + count; }

  const T* data() const { return mem; }
  std::size_t size() const { return count; }

  const T& operator[](std::size_t i) { return mem[i]; }
};

template <typename T, typename V> static span<V> to_span(const T& val);
template <typename T> static span<T> to_span(const std::vector<T>& val)
{
  return span(val.data(), val.size());
}

template <typename T> static span<char> byte_view(const T& val)
{
  return span<char>(reinterpret_cast<const char*>(&val), sizeof(val));
}

/**
 * A proto node is the is an internal node of the on-disk-tree.
 * It itself does not contain any data but points to where the data lies
 * on disk.
 */
struct proto_node {
  struct {
    // since findex is only (signed) 32bit, (unsigned) 32bit here should
    // be enough to account for not-backedup-files
    // indices into the proto node array.
    // If this node lies at index i, then its subtree are all nodes
    // i+1 <= j < end.
    std::uint32_t size;
  } sub;

  std::uint32_t padding{0};

  struct {
    // byte offsets relative to the start of the string area
    std::uint64_t start : 48;
    std::uint64_t length : 16;
  } name;

  // deltas only make sense when adding up multiple jobs.  Since
  // this storage format is only to be used for one job, we can savely remove
  // this.
  // in case we want to support this, we can probably just add an index to each
  // delta pair instead; on a restore we would iterate over each pair and add
  // the pair to the corresponding node.
  struct {
    std::uint64_t start;
    std::uint64_t end;  // TODO: 32bit length enough here ?
  } deltas;

  static std::uint64_t make_id(int32_t findex, uint32_t jobid)
  {
    std::uint64_t low = 0;
    std::memcpy(&low, &findex, sizeof(findex));
    std::uint64_t high = jobid;
    return high << 32 | low;
  }
};

struct children {
  using idx_type = std::ptrdiff_t;

  struct child_iter {
    idx_type idx;
    const proto_node* nodes;

    child_iter& operator++()
    {
      idx += nodes[idx].sub.size + 1;
      return *this;
    }

    bool operator!=(const child_iter& other) const
    {
      return idx != other.idx || nodes != other.nodes;
    }

    idx_type operator*() const { return idx; }
  };

  child_iter begin() const { return child_iter{1, nodes}; }

  child_iter end() const { return child_iter{nodes[0].sub.size, nodes}; }

  const proto_node* nodes;

  children(const proto_node& node) : nodes{&node} {}
};

static_assert(std::is_same_v<JobId_t, std::uint32_t>);

struct delta_part {
  std::uint32_t jobid;
  std::int32_t findex;
};

struct meta_data {
  std::int32_t findex; /* file index */
  std::uint32_t jobid; /* JobId */
  struct {
    std::uint32_t type : 8;      /* node type */
    std::uint32_t hard_link : 1; /* set if have hard link */
    std::uint32_t soft_link : 1; /* set if is soft link */
  };

  std::uint32_t original;  // uint32_max = no original; else index of original

  static constexpr std::uint32_t original_not_found
      = std::numeric_limits<decltype(original)>::max();
};

struct fh {
  std::uint64_t node{0};
  std::uint64_t info{0};
};

struct delta {
  std::int32_t seq_num{0};
};

struct tree_header {
  static constexpr std::uint64_t offset_not_found = 0;
  // todo: add jobid(s)?
  std::uint64_t num_nodes;
  std::uint64_t nodes_offset;
  std::uint64_t string_offset;
  std::uint64_t string_size;
  std::uint64_t meta_offset;
  std::uint64_t fh_offset;
  std::uint64_t seq_offset;
  std::uint64_t delta_offset;
  std::uint64_t delta_count;
  std::uint64_t checksum_offset;
  std::uint64_t checksum_size;
};

struct tree_view {
  // This struct does not hold any data!
  // it just gives a convinient view into the stored tree data
  span<proto_node> nodes{};
  // probably best to have one span per important meta field,
  // i.e. type, hardlink (offset of original hl file), id (= findex | jobid)
  // other stuff is not necessary.
  span<meta_data> metas{};
  span<delta_part> deltas{};
  span<delta> delta_seqs{};
  span<fh> fhs{};
  std::string_view string_pool{};
  span<char> chksum{};

  tree_view() {}
  tree_view(span<char> bytes)
  {
    tree_header header;
    std::memcpy(&header, bytes.data(), sizeof(header));

    auto count = header.num_nodes;
    {
      auto* start = (const proto_node*)(bytes.data() + header.nodes_offset);
      nodes = span(start, count);
    }
    {
      auto* start = (const meta_data*)(bytes.data() + header.meta_offset);
      metas = span(start, count);
    }
    if (header.fh_offset != header.offset_not_found) {
      auto* start = (const fh*)(bytes.data() + header.fh_offset);
      fhs = span(start, header.num_nodes);
    }
    if (header.seq_offset != header.offset_not_found) {
      auto* start = (const delta*)(bytes.data() + header.seq_offset);
      delta_seqs = span(start, header.num_nodes);
    }
    {
      auto* start = (const delta_part*)(bytes.data() + header.delta_offset);
      deltas = span(start, header.delta_count);
    }
    {
      const char* start = (const char*)(bytes.data() + header.string_offset);
      std::size_t size = header.string_size;

      string_pool = std::string_view(start, size);
    }

    {
      auto* start = (const char*)(bytes.data() + header.checksum_offset);
      chksum = span(start, header.checksum_size);
    }
  }

  std::size_t size() const { return nodes.size(); }

  std::string_view name(size_t i)
  {
    auto& node = nodes[i];
    auto name_size = node.name.length;
    auto* start = string_pool.data() + node.name.start;
    return std::string_view{start, name_size};
  }

  std::int32_t delta_seq_at(std::size_t i)
  {
    if (delta_seqs.data()) {
      return delta_seqs[i].seq_num - (metas[i].type == TN_NEWDIR);
    } else {
      return -(metas[i].type == TN_NEWDIR);
    }
  }

  fh fh_at(std::size_t i)
  {
    if (fhs.data()) {
      return fhs[i];
    } else {
      return fh{};
    }
  }
};

struct tree_builder {
  std::vector<proto_node> nodes;
  std::vector<char> string_area;
  std::vector<meta_data> metas;
  std::vector<delta> delta_seqs;
  std::vector<fh> fhs;
  std::vector<delta_part> deltas;

  using node_map = std::unordered_map<const TREE_NODE*, std::size_t>;

  // TREE_ROOT cannot be const because lookups in hardlinks is not
  // const :/
  tree_builder(TREE_ROOT* root)
  {
    node_map m;
    {
      TREE_NODE* root_node = reinterpret_cast<TREE_NODE*>(root);
      TREE_NODE* child;
      foreach_child (child, root_node) { build(m, child); }
    }

    for (auto& meta : metas) {
      std::uint64_t key = meta.jobid;
      key <<= 32;
      key |= meta.findex;

      // this does not work when using optimize_for_size!
      HL_ENTRY* entry = root->hardlinks.lookup(key);
      if (entry && entry->node) {
        meta.original = m[entry->node];
      } else {
        meta.original = meta.original_not_found;
      }
    }
  }

  std::vector<char> to_bytes()
  {
    std::vector<char> data;

    simple_checksum chk{};

    static_assert(sizeof(tree_header) % alignof(proto_node) == 0);

    data.resize(sizeof(tree_header));

    {
      std::size_t align = alignof(proto_node);
      std::size_t current = data.size();
      std::size_t a = current + align - 1;
      std::size_t next = (a / align) * align;
      ASSERT(next >= current);
      data.resize(next);
    }

    auto node_offset = data.size();
    {
      auto b = (const char*)nodes.data();
      auto e = (const char*)(nodes.data() + nodes.size());
      data.insert(data.end(), b, e);
      chk.add(b, e);
    }
    Dmsg0(10, "Nodes: %llu - %llu (%llu)\n", node_offset, data.size(),
          data.size() - node_offset);

    auto string_offset = data.size();

    data.insert(data.end(), string_area.begin(), string_area.end());
    chk.add(&*string_area.cbegin(), &*string_area.cend());

    Dmsg0(10, "Strings: %llu - %llu (%llu)\n", string_offset, data.size(),
          data.size() - string_offset);

    {
      std::size_t align = alignof(meta_data);
      std::size_t current = data.size();
      std::size_t a = current + align - 1;
      std::size_t next = (a / align) * align;
      ASSERT(next >= current);
      data.resize(next);
    }

    auto meta_offset = data.size();
    {
      auto b = (const char*)metas.data();
      auto e = (const char*)(metas.data() + metas.size());
      auto bytes = span(b, e - b);
      data.insert(data.end(), bytes.begin(), bytes.end());
      chk.add(bytes.begin(), bytes.end());
    }
    Dmsg0(10, "Meta: %llu - %llu (%llu)\n", meta_offset, data.size(),
          data.size() - meta_offset);

    {
      std::size_t align = alignof(fh);
      std::size_t current = data.size();
      std::size_t a = current + align - 1;
      std::size_t next = (a / align) * align;
      ASSERT(next >= current);
      data.resize(next);
    }

    auto fh_offset = data.size();
    if (std::find_if(fhs.begin(), fhs.end(),
                     [](auto&& fh) { return fh.info != 0 || fh.node != 0; })
        != fhs.end()) {
      auto b = (const char*)fhs.data();
      auto e = (const char*)(fhs.data() + fhs.size());
      auto bytes = span(b, e - b);
      data.insert(data.end(), bytes.begin(), bytes.end());
      chk.add(bytes.begin(), bytes.end());
    } else {
      fh_offset = tree_header::offset_not_found;
    }

    if (fh_offset != tree_header::offset_not_found) {
      Dmsg0(10, "fh: %llu - %llu (%llu)\n", fh_offset, data.size(),
            data.size() - fh_offset);
    }

    {
      std::size_t align = alignof(delta);
      std::size_t current = data.size();
      std::size_t a = current + align - 1;
      std::size_t next = (a / align) * align;
      ASSERT(next >= current);
      data.resize(next);
    }

    auto seq_offset = data.size();
    if (std::find_if(delta_seqs.begin(), delta_seqs.end(),
                     [](auto&& delta_seq) { return delta_seq.seq_num != 0; })
        != delta_seqs.end()) {
      auto b = (const char*)delta_seqs.data();
      auto e = (const char*)(delta_seqs.data() + delta_seqs.size());
      auto bytes = span(b, e - b);
      data.insert(data.end(), bytes.begin(), bytes.end());
      chk.add(bytes.begin(), bytes.end());
    } else {
      seq_offset = tree_header::offset_not_found;
    }
    if (seq_offset != tree_header::offset_not_found) {
      Dmsg0(10, "delta_seq: %llu - %llu (%llu)\n", seq_offset, data.size(),
            data.size() - seq_offset);
    }

    {
      std::size_t align = alignof(delta_part);
      std::size_t current = data.size();
      std::size_t a = current + align - 1;
      std::size_t next = (a / align) * align;
      ASSERT(next >= current);
      data.resize(next);
    }

    auto delta_offset = data.size();
    {
      auto b = (const char*)deltas.data();
      auto e = (const char*)(deltas.data() + deltas.size());
      auto bytes = span(b, e - b);
      data.insert(data.end(), bytes.begin(), bytes.end());
      chk.add(bytes.begin(), bytes.end());
    }
    Dmsg0(10, "deltas: %llu - %llu (%llu)\n", delta_offset, data.size(),
          data.size() - delta_offset);

    std::vector chksum = chk.finalize();
    {
      std::size_t align = alignof(*chksum.data());
      std::size_t current = data.size();
      std::size_t a = current + align - 1;
      std::size_t next = (a / align) * align;
      ASSERT(next >= current);
      data.resize(next);
    }

    auto chk_off = data.size();
    {
      auto b = (const char*)chksum.data();
      auto e = (const char*)(chksum.data() + chksum.size());
      auto bytes = span(b, e - b);
      data.insert(data.end(), bytes.begin(), bytes.end());
    }


    tree_header header{.num_nodes = nodes.size(),
                       .nodes_offset = node_offset,
                       .string_offset = string_offset,
                       .string_size = string_area.size(),
                       .meta_offset = meta_offset,
                       .fh_offset = fh_offset,
                       .seq_offset = seq_offset,
                       .delta_offset = delta_offset,
                       .delta_count = deltas.size(),
                       .checksum_offset = chk_off,
                       .checksum_size = chksum.size()};

    auto header_bytes = byte_view(header);
    ASSERT(header_bytes.size() == sizeof(tree_header));
    std::memcpy(data.data(), header_bytes.data(), header_bytes.size());
    return data;
  }

 private:
  void build(node_map& m, const TREE_NODE* node)
  {
    if (!node) { return; }
    std::size_t pos = nodes.size();
    m[node] = pos;
    {
      proto_node& n = nodes.emplace_back();

      n.name.start = string_area.size();
      string_area.insert(string_area.end(), node->fname,
                         node->fname + strlen(node->fname));
      n.name.length = string_area.size() - n.name.start;

      meta_data& meta = metas.emplace_back();
      meta.findex = node->FileIndex;
      meta.jobid = node->JobId;
      meta.type = node->type;
      meta.hard_link = node->hard_link;
      meta.soft_link = node->soft_link;

      delta& delta_seq = delta_seqs.emplace_back();
      delta_seq.seq_num = node->delta_seq + node->type == TN_NEWDIR;

      fh& fh = fhs.emplace_back();
      fh.node = node->fhnode;
      fh.info = node->fhinfo;

      n.deltas.start = deltas.size();
      for (auto* delta = node->delta_list; delta; delta = delta->next) {
        delta_part& part = deltas.emplace_back();
        part.jobid = delta->JobId;
        part.findex = delta->FileIndex;
      }
      n.deltas.end = deltas.size();
    }

    std::size_t start = nodes.size();

    TREE_NODE* child;
    foreach_child (child, const_cast<TREE_NODE*>(node)) { build(m, child); }

    std::size_t end = nodes.size();

    nodes[pos].sub.size = end - start;
  }
};

#include <fstream>

bool MakeFileWithContents(const char* path, span<char> content)
{
  std::ofstream output(path, std::ios::binary);

  if (!output) { return false; }

  output.exceptions(output.exceptions() | std::ios::failbit | std::ios::badbit);

  try {
    output.write(content.data(), content.size());
    return true;
  } catch (const std::system_error& e) {
    Dmsg3(100, "Caught system error: [%s:%d] ERR=%s\n",
          e.code().category().name(), e.code().value(), e.what());
  } catch (const std::exception& e) {
    Dmsg0(100, "Caught exception: %s\n", e.what());
  } catch (...) {
    Dmsg0(30, "Caught something that was not an exception.\n");
  }
  return false;
}

std::vector<char> LoadFile(const char* path)
{
  std::ifstream input(path, std::ios::binary);
  input.exceptions(std::ios_base::badbit);

  if (!input) { throw std::ios_base::failure("error opening file"); }

  std::vector<char> content;
  auto current = content.size();
  std::size_t read_size = 256 * 1024;
  do {
    current += input.gcount();
    content.resize(current + read_size);
  } while (input.read(content.data() + current, read_size));
  content.resize(current + input.gcount());

  return content;
}

bool SaveTree(const char* path, TREE_ROOT* root)
{
  std::vector<char> bytes = tree_builder(root).to_bytes();

  return MakeFileWithContents(path, to_span(bytes));
}

// TODO: share this with the real routines
#define B_PAGE_SIZE 4096
#define MAX_PAGES 2400
#define MAX_BUF_SIZE (MAX_PAGES * B_PAGE_SIZE) /* approx 10MB */

static void MallocBuf(TREE_ROOT* root, int size)
{
  struct s_mem* mem;

  mem = (struct s_mem*)malloc(size);
  root->total_size += size;
  root->blocks++;
  mem->next = root->mem;
  root->mem = mem;
  mem->mem = mem->first;
  mem->rem = (char*)mem + size - (char*)mem->mem;
  Dmsg2(200, "malloc buf size=%d rem=%d\n", size, mem->rem);
}

static char* RootMalloc(TREE_ROOT* root, int size)
{
  struct s_mem* mem;

  mem = (struct s_mem*)malloc(size + sizeof(struct s_mem));
  root->total_size += size + sizeof(struct s_mem);
  root->blocks++;
  if (root->mem) {
    mem->next = root->mem->next;
    root->mem->next = mem;
  } else {
    mem = root->mem;
    root->mem = mem;
  }
  mem->mem = mem->first;
  mem->rem = 0;
  Dmsg2(200, "malloc buf size=%d rem=%d\n", size, mem->rem);
  return (char*)mem + sizeof(struct s_mem);
}

template <typename T> static T* tree_alloc(TREE_ROOT* root, int size)
{
  T* buf;
  int asize = BALIGN(size);

  if (root->mem->rem < asize) {
    uint32_t mb_size;
    if (root->total_size >= (MAX_BUF_SIZE / 2)) {
      mb_size = MAX_BUF_SIZE;
    } else {
      mb_size = MAX_BUF_SIZE / 2;
    }
    MallocBuf(root, mb_size);
  }
  root->mem->rem -= asize;
  buf = static_cast<T*>(root->mem->mem);
  root->mem->mem = (char*)root->mem->mem + asize;
  return buf;
}

static int NodeCompare(void* item1, void* item2)
{
  TREE_NODE* tn1 = (TREE_NODE*)item1;
  TREE_NODE* tn2 = (TREE_NODE*)item2;

  if (tn1->fname[0] > tn2->fname[0]) {
    return 1;
  } else if (tn1->fname[0] < tn2->fname[0]) {
    return -1;
  }

  return strcmp(tn1->fname, tn2->fname);
}

inline void PrintTree(TREE_NODE* node, std::string& s, int depth = 0)
{
  for (int i = 0; i < depth; ++i) { s += "*"; }

  s += "* ";

  s += node->fname;
  s += "\n";

  TREE_NODE* child;
  foreach_child (child, node) { PrintTree(child, s, depth + 1); }
}

TREE_ROOT* tree_from_view(tree_view tree, bool mark)
{
  TREE_ROOT* root = new_tree(tree.size());

  TREE_NODE* nodes
      = (TREE_NODE*)RootMalloc(root, tree.size() * sizeof(TREE_NODE));
  delta_list* deltas
      = (delta_list*)RootMalloc(root, tree.deltas.size() * sizeof(delta_list));

  // this can probably be done in the othre direction too!
  for (std::size_t i = tree.size(); i > 0;) {
    i -= 1;
    const proto_node& pnode = tree.nodes[i];
    TREE_NODE& node = *new (&nodes[i]) TREE_NODE;
    auto str = tree.nodes[i].name;
    node.fname = tree_alloc<char>(root, str.length + 1);
    std::memcpy(node.fname, tree.string_pool.data() + str.start, str.length);
    node.fname[str.length] = 0;
    node.parent = (TREE_NODE*)root;
    for (auto child : children(pnode)) {
      auto& childnode = nodes[i + child];
      node.child.insert(&childnode, NodeCompare);
      childnode.parent = &node;
    }
  }

  root->first = &nodes[0];
  root->last = &nodes[tree.size() - 1];
  root->last->next = nullptr;
  root->child.insert(&nodes[0], NodeCompare);

  for (std::size_t i = 0; i < tree.size(); ++i) {
    auto& saved = tree.nodes[i];
    TREE_NODE& current = nodes[i];

    auto& meta = tree.metas[i];
    if (i < tree.size() - 1) {
      current.next = &nodes[i + 1];
    } else {
      current.next = nullptr;
    }
    current.FileIndex = meta.findex;
    current.JobId = meta.jobid;

    current.delta_seq = tree.delta_seq_at(i);

    auto fh = tree.fh_at(i);
    current.fhinfo = fh.info;
    current.fhnode = fh.node;

    auto str = saved.name;
    std::memcpy(current.fname, tree.string_pool.data() + str.start, str.length);
    current.fname[str.length] = 0;

    current.type = meta.type;
    current.extract = 0;
    current.extract_dir = 0;
    if (mark) {
      current.extract = true;
      if (meta.type == TN_DIR || meta.type == TN_DIR_NLS) {
        current.extract_dir = true;
      }
    }
    current.hard_link = meta.hard_link;
    current.soft_link = meta.soft_link;

    if (meta.original == meta.original_not_found) {
      auto* entry = (HL_ENTRY*)root->hardlinks.hash_malloc(sizeof(HL_ENTRY));
      entry->key = (((uint64_t)meta.jobid) << 32) + meta.findex;
      entry->node = &current;
      root->hardlinks.insert(entry->key, entry);
    } else {
      // Then add hardlink entry to linked node.
      auto* entry = (HL_ENTRY*)root->hardlinks.hash_malloc(sizeof(HL_ENTRY));
      entry->key = (((uint64_t)meta.jobid) << 32) + meta.findex;
      entry->node = &nodes[meta.original];
      root->hardlinks.insert(entry->key, entry);
    }

    if (saved.deltas.start != saved.deltas.end) {
      for (auto cur = saved.deltas.start; cur < saved.deltas.end; ++cur) {
        auto& delta = deltas[cur];
        auto& saved_delta = tree.deltas[cur];
        delta.JobId = saved_delta.jobid;
        delta.FileIndex = saved_delta.findex;
        if (cur != saved.deltas.end - 1) {
          delta.next = &delta + 1;
        } else {
          delta.next = nullptr;
        }
      }
      current.delta_list = &deltas[saved.deltas.start];
    } else {
      current.delta_list = nullptr;
    }
  }

  return root;
}

bool CheckTree(const tree_view& view)
{
  if (view.version != current_version) { return false; }
  try {
    std::vector chksum
        = simple_checksum{}
              .add((const char*)view.nodes.begin(),
                   (const char*)view.nodes.end())
              .add((const char*)view.string_pool.begin(),
                   (const char*)view.string_pool.end())
              .add((const char*)view.metas.begin(),
                   (const char*)view.metas.end())
              .add((const char*)view.fhs.begin(), (const char*)view.fhs.end())
              .add((const char*)view.delta_seqs.begin(),
                   (const char*)view.delta_seqs.end())
              .add((const char*)view.deltas.begin(),
                   (const char*)view.deltas.end())
              .finalize();


    if (chksum.size() == view.chksum.size()
        && memcmp(chksum.data(), view.chksum.data(), chksum.size()) == 0) {
      return true;
    }
  } catch (const std::exception& e) {
    Dmsg1(50, "Error while computing checksum: %s\n", e.what());
  }
  return false;
}


TREE_ROOT* LoadTree(const char* path, std::size_t* size, bool marked_initially)
{
  try {
    std::vector<char> bytes = LoadFile(path);

    tree_view view(to_span(bytes));

    if (!CheckTree(view)) { return nullptr; }

    *size = view.size();
    return tree_from_view(view, marked_initially);
  } catch (const std::exception& e) {
    Dmsg1(50, "Error while loading tree from %s: ERR=%s\n", path, e.what());
    *size = 0;
    return nullptr;
  }
}

struct map_ptr {
  void* ptr{nullptr};
  std::size_t size{0};

  map_ptr(const char* path)
  {
    int fd = open(path, O_RDONLY);

    if (fd < 0) { throw std::ios_base::failure("error opening file"); }

    struct stat s;
    if (fstat(fd, &s) < 0) {
      throw std::ios_base::failure("error stating file");
    }

    size = s.st_size;
    ptr = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED) {
      throw std::ios_base::failure("error mmaping file");
    }

    if (madvise(ptr, size, MADV_WILLNEED) != 0) {
      throw std::ios_base::failure("error madvising mmap");
    }

    ::close(fd);
  }

  map_ptr(map_ptr&& other) { *this = std::move(other); }

  map_ptr& operator=(map_ptr&& other)
  {
    std::swap(ptr, other.ptr);
    std::swap(size, other.size);
    return *this;
  }

  span<char> to_span() { return span{static_cast<char*>(ptr), size}; }

  ~map_ptr()
  {
    if (ptr) { munmap(ptr, size); }
  }
};

class NewTree {
  using bytes = std::vector<char>;

 public:
  std::vector<bytes> tree_data;
  std::vector<map_ptr> tree_mmap;
  std::vector<tree_view> views;
};

void DeleteTree(NewTree* nt) { nt->~NewTree(); }

tree_ptr MakeNewTree() { return tree_ptr(new NewTree); }

bool AddTree(NewTree* tree, const char* path)
{
  try {
#if 1
    std::vector<char> bytes = LoadFile(path);

    tree_view view(to_span(bytes));

    if (!CheckTree(view)) { return false; }

    tree->tree_data.emplace_back(std::move(bytes));
    tree->views.push_back(view);
#else
    map_ptr map(path);

    tree_view view(map.to_span());

    if (!CheckTree(view)) { return false; }

    tree->tree_mmap.emplace_back(std::move(map));
    tree->views.push_back(view);

#endif

    return true;
  } catch (const std::exception& e) {
    Dmsg1(50, "Error while loading tree from %s: ERR=%s\n", path, e.what());
    return false;
  }
}

static bool InsertNode(TREE_ROOT* root,
                       int32_t LinkFI,
                       bool hardlinked,
                       bool soft_link,
                       int FileIndex,
                       int32_t delta_seq,
                       JobId_t JobId,
                       const char* Path,
                       const char* File,
                       int type,
                       int fhinfo,
                       int fhnode,
                       bool all)
{
  if (type == TN_ROOT) { return false; }

  int ntype = [&] {
    if (*File != 0) {
      return TN_FILE;
    } else if (!IsPathSeparator(*Path)) {
      return TN_DIR_NLS;
    } else {
      return TN_DIR;
    }
  }();

  ASSERT(ntype == type);

  bool hard_link = LinkFI != 0;
  auto* node = insert_tree_node((char*)Path, (char*)File, type, root, NULL);

  node->fhinfo = fhinfo;
  node->fhnode = fhnode;

  if (delta_seq > 0) {
    if (delta_seq == (node->delta_seq + 1)) {
      TreeAddDeltaPart(root, node, node->JobId, node->FileIndex);

    } else {
      /* File looks to be deleted */
      if (node->delta_seq == -1) { /* just created */
        TreeRemoveNode(root, node);

      } else {
        Dmsg3(0,
              "Something is wrong with Delta, skip it "
              "fname=%s d1=%d d2=%d\n",
              File, node->delta_seq, delta_seq);
      }
      return false;
    }
  }

  /* - The first time we see a file (node->inserted==true), we accept it.
   * - In the same JobId, we accept only the first copy of a
   *   hard linked file (the others are simply pointers).
   * - In the same JobId, we accept the last copy of any other
   *   file -- in particular directories.
   *
   * All the code to set ok could be condensed to a single
   * line, but it would be even harder to read. */
  bool ok = true;
  if (!node->inserted && JobId == node->JobId) {
    if ((hard_link && FileIndex > node->FileIndex)
        || (!hard_link && FileIndex < node->FileIndex)) {
      ok = false;
    }
  }
  if (ok) {
    node->hard_link = hard_link;
    node->FileIndex = FileIndex;
    node->JobId = JobId;
    node->type = type;
    node->soft_link = soft_link;
    node->delta_seq = delta_seq;

    if (all) {
      node->extract = true; /* extract all by default */
      if (type == TN_DIR || type == TN_DIR_NLS) {
        node->extract_dir = true; /* if dir, extract it */
      }
    }

    // Insert file having hardlinks into hardlink hashtable.
    if (hardlinked && type != TN_DIR && type != TN_DIR_NLS) {
      if (!LinkFI) {
        // First occurence - file hardlinked to
        auto* entry = (HL_ENTRY*)root->hardlinks.hash_malloc(sizeof(HL_ENTRY));
        entry->key = (((uint64_t)JobId) << 32) + FileIndex;
        entry->node = node;
        root->hardlinks.insert(entry->key, entry);
      } else {
        // Hardlink to known file index: lookup original file
        uint64_t file_key = (((uint64_t)JobId) << 32) + LinkFI;
        HL_ENTRY* first_hl = (HL_ENTRY*)root->hardlinks.lookup(file_key);

        if (first_hl && first_hl->node) {
          // Then add hardlink entry to linked node.
          auto* entry
              = (HL_ENTRY*)root->hardlinks.hash_malloc(sizeof(HL_ENTRY));
          entry->key = (((uint64_t)JobId) << 32) + FileIndex;
          entry->node = first_hl->node;
          root->hardlinks.insert(entry->key, entry);
        }
      }
    }
  }

  return true;
}

TREE_ROOT* CombineTree(tree_ptr tree, std::size_t* count, bool mark_on_load)
{
  size_t int_count = 0;
  for (auto& view : tree->views) { int_count += view.size(); }

  std::string Path{"/"}, File;
  TREE_ROOT* root = new_tree(int_count);
  std::vector<size_t> path_stack;

  for (auto& view : tree->views) {
    path_stack.clear();
    Path.assign("/");
    for (size_t i = 0; i < view.size(); ++i) {
      while (path_stack.size()) {
        ASSERT(Path.size() > 0);
        if (i >= path_stack.back()) {
          Path.pop_back();  // remove last /
          auto pos = Path.find_last_of('/');
          ASSERT(pos != Path.npos);
          Path.resize(pos + 1);  // keep /
          path_stack.pop_back();
        } else {
          break;
        }
      }
      auto& node = view.nodes[i];

      auto& meta = view.metas[i];
      int type = meta.type;
      auto fname = view.name(i);
      if (type != TN_FILE) {
        Path.append(fname);
        Path += "/";
        auto end = i + 1 + node.sub.size;
        if (path_stack.size()) { ASSERT(path_stack.back() >= end); }
        path_stack.push_back(end);
        File.clear();
      } else {
        File.assign(fname);
      }

      if (type == TN_NEWDIR) continue;
      auto FileIndex = meta.findex;
      auto delta_seq = view.delta_seq_at(i);
      auto fh = view.fh_at(i);
      auto fhinfo = fh.info;
      auto fhnode = fh.node;
      auto JobId = meta.jobid;
      int LinkFI = 0;
      bool hardlinked = false;
      if (meta.original == i) {
        hardlinked = true;  // we are the original hardlink
      } else if (meta.original != meta.original_not_found) {
        LinkFI = view.metas[meta.original].findex;  // we are not the original
        hardlinked = true;
      }
      bool soft_link = meta.soft_link;

      const char* path = Path.c_str();
      if (type == TN_DIR_NLS) {
        // remove leading slash
        path += 1;
      }
      if (!InsertNode(root, LinkFI, hardlinked, soft_link, FileIndex, delta_seq,
                      JobId, path, File.c_str(), type, fhinfo, fhnode,
                      mark_on_load)) {
        int_count -= 1;
      }
    }
  }

  *count = int_count;
  return root;
}
