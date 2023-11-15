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

#include <cstring>
#include <cstdint>

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
    // indices into the proto node array
    // start and end of subtree (start is the first child!)
    std::uint64_t start;  // start is useless; its always the next one
    std::uint64_t end;
  } sub;

  std::uint64_t child_begin() { return sub.start; }

  std::uint64_t next() { return sub.end; }

  std::uint64_t child_end() { return sub.end; }

  struct {
    // byte offsets relative to the start of the string area
    std::uint64_t start;
    std::uint64_t end;
  } name;

  struct {
    // index into the meta data array
    std::uint64_t meta;
  } misc;

  struct {
    std::uint64_t start;
    std::uint64_t end;
  } deltas;

  static std::uint64_t make_id(int32_t findex, uint32_t jobid)
  {
    std::uint64_t low = 0;
    std::memcpy(&low, &findex, sizeof(findex));
    std::uint64_t high = jobid;
    return high << 32 | low;
  }
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

  std::uint64_t delta_seq;
  std::uint64_t fhnode;
  std::uint64_t fhinfo;
  std::int64_t original;  // -1 = no original; else index of original
};

struct tree_header {
  // todo: checksum ?
  std::uint64_t num_nodes;
  std::uint64_t nodes_offset;
  std::uint64_t string_offset;
  std::uint64_t meta_offset;
  std::uint64_t delta_offset;
  std::uint64_t delta_count;
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
  std::string_view string_pool{};

  tree_view() {}
  tree_view(span<char> bytes)
  {
    tree_header header;
    std::memcpy(&header, bytes.data(), sizeof(header));

    auto count = header.num_nodes;
    {
      const proto_node* start
          = (const proto_node*)(bytes.data() + header.nodes_offset);
      nodes = span(start, count);
    }
    {
      const meta_data* start
          = (const meta_data*)(bytes.data() + header.meta_offset);
      metas = span(start, count);
    }
    {
      const delta_part* start
          = (const delta_part*)(bytes.data() + header.delta_offset);
      deltas = span(start, header.delta_count);
    }
    {
      const char* start = (const char*)(bytes.data() + header.string_offset);
      std::size_t size = bytes.size() - header.string_offset;

      string_pool = std::string_view(start, size);
    }
  }

  std::size_t size() const { return nodes.size(); }

  std::string_view name(size_t i)
  {
    auto& node = nodes[i];
    auto name_size = node.name.end - node.name.start;
    auto* start = string_pool.data() + node.name.start;
    return std::string_view{start, name_size};
  }
};

struct tree_builder {
  std::vector<proto_node> nodes;
  std::vector<char> string_area;
  std::vector<meta_data> metas;
  std::vector<delta_part> deltas;

  using node_map = std::unordered_map<const TREE_NODE*, std::size_t>;

  // TREE_ROOT cannot be const because lookups in hardlinks is not
  // const :/
  tree_builder(TREE_ROOT* root)
  {
    node_map m;
    build(m, root->first);
    for (auto& meta : metas) {
      std::uint64_t key = meta.jobid;
      key <<= 32;
      key |= meta.findex;

      HL_ENTRY* entry = root->hardlinks.lookup(key);
      if (entry && entry->node) {
        meta.original = m[entry->node];
      } else {
        meta.original = -1;
      }
    }
  }

  std::vector<char> to_bytes()
  {
    std::vector<char> data;

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
    for (auto& node : nodes) {
      auto bytes = byte_view(node);
      data.insert(data.end(), bytes.begin(), bytes.end());
    }

    auto string_offset = data.size();

    data.insert(data.end(), string_area.begin(), string_area.end());

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
    }

    tree_header header{nodes.size(), node_offset,  string_offset,
                       meta_offset,  delta_offset, deltas.size()};

    auto header_bytes = byte_view(header);
    for (size_t i = 0; i < header_bytes.size(); ++i) {
      data[i] = header_bytes[i];
    }
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
      n.name.end = string_area.size();

      n.misc.meta = metas.size();

      meta_data& meta = metas.emplace_back();
      meta.findex = node->FileIndex;
      meta.jobid = node->JobId;
      meta.type = node->type;
      meta.hard_link = node->hard_link;
      meta.soft_link = node->soft_link;
      meta.delta_seq = node->delta_seq;
      meta.fhnode = node->fhnode;
      meta.fhinfo = node->fhinfo;

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

    nodes[pos].sub.start = start;
    nodes[pos].sub.end = end;
  }
};

#include <fstream>

bool MakeFileWithContents(const char* path, span<char> content)
{
  std::ofstream output(path, std::ios::binary);

  if (!output) { return false; }

  try {
    output.write(content.data(), content.size());
    return true;
  } catch (...) {
    return false;
  }
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
    node.fname = tree_alloc<char>(root, str.end - str.start + 1);
    std::memcpy(node.fname, tree.string_pool.data() + str.start,
                str.end - str.start);
    node.fname[str.end - str.start] = 0;
    node.parent = (TREE_NODE*)root;
    for (std::size_t child = pnode.sub.start; child < pnode.sub.end;
         child = tree.nodes[child].sub.end) {
      auto& childnode = nodes[child];
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

    auto& meta = tree.metas[saved.misc.meta];
    if (i < tree.size() - 1) {
      current.next = &nodes[i + 1];
    } else {
      current.next = nullptr;
    }
    current.FileIndex = meta.findex;
    current.JobId = meta.jobid;
    current.delta_seq = meta.delta_seq;
    current.fhinfo = meta.fhinfo;
    current.fhnode = meta.fhnode;

    auto str = saved.name;
    std::memcpy(current.fname, tree.string_pool.data() + str.start,
                str.end - str.start);
    current.fname[str.end - str.start] = 0;

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

    if (meta.original < 0) {
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

TREE_ROOT* LoadTree(const char* path, std::size_t* size, bool marked_initially)
{
  try {
    std::vector<char> bytes = LoadFile(path);

    tree_view view(to_span(bytes));

    *size = view.size();
    return tree_from_view(view, marked_initially);
  } catch (const std::exception& e) {
    Dmsg1(50, "Error while loading tree from %s: ERR=%s\n", path, e.what());
    *size = 0;
    return nullptr;
  }
}

class NewTree {
  using bytes = std::vector<char>;

 public:
  std::vector<bytes> tree_data;
  std::vector<tree_view> views;
};

void DeleteTree(const NewTree* nt) { nt->~NewTree(); }

tree_ptr MakeNewTree() { return tree_ptr(new NewTree); }

bool AddTree(NewTree* tree, const char* path)
{
  try {
    std::vector<char> bytes = LoadFile(path);


    tree_view view(to_span(bytes));

    tree->tree_data.emplace_back(std::move(bytes));
    tree->views.push_back(view);

    return true;
  } catch (const std::exception& e) {
    Dmsg1(50, "Error while loading tree from %s: ERR=%s\n", path, e.what());
    return false;
  }
}

static bool InsertNode(TREE_ROOT* root,
                       int32_t LinkFI,
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
    if (type != TN_DIR && type != TN_DIR_NLS) {
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

      auto j = node.misc.meta;
      auto& meta = view.metas[j];
      int type = meta.type;
      auto fname = view.name(i);
      if (type != TN_FILE) {
        Path.append(fname);
        Path += "/";
        if (path_stack.size()) { ASSERT(path_stack.back() >= node.sub.end); }
        path_stack.push_back(node.sub.end);
        File.clear();
      } else {
        File.assign(fname);
      }

      if (type == TN_NEWDIR) continue;
      auto FileIndex = meta.findex;
      auto delta_seq = meta.delta_seq;
      auto fhinfo = meta.fhinfo;
      auto fhnode = meta.fhnode;
      auto JobId = meta.jobid;
      int LinkFI = 0;
      if (meta.original >= 0) {
        auto oj = view.nodes[meta.original].misc.meta;
        LinkFI = view.metas[oj].findex;
      }
      bool soft_link = meta.soft_link;

      if (!InsertNode(root, LinkFI, soft_link, FileIndex, delta_seq, JobId,
                      Path.c_str(), File.c_str(), type, fhinfo, fhnode,
                      mark_on_load)) {
        int_count -= 1;
      }
    }
  }

  *count = int_count;
  return root;
}
