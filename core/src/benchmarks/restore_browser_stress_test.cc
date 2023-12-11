/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "benchmark/benchmark.h"
#else
#  include "include/bareos.h"
#  include "benchmark/benchmark.h"
#endif
#include <iostream>
#define private public
#include "dird/ua_tree.cc"
#include "dird/ua_output.h"
#include "dird/ua.h"
#include "dird/ua_restore.cc"
#include <cstring>
#include <memory_resource>

#include <utility>
#include <numeric>

using namespace directordaemon;

template <typename T> struct array_list {
  using storage_type = std::aligned_storage_t<sizeof(T), alignof(T)>;

  std::unique_ptr<array_list> next;
  std::size_t capacity{0};
  std::size_t size{0};
  std::unique_ptr<storage_type[]> data;

  array_list(std::size_t capacity = 100) : capacity{capacity}
  {
    data = std::make_unique<storage_type[]>(capacity);
  }

  array_list(array_list&& l)
      : next{std::move(l.next)}
      , capacity{std::move(l.capacity)}
      , size{std::move(l.size)}
      , data{std::move(l.data)}
  {
  }

  template <typename... Args> T& emplace_back(Args... args)
  {
    if (size >= capacity) {
      std::unique_ptr<array_list> copy
          = std::make_unique<array_list<T>>(std::move(*this));

      next = std::move(copy);
      size = 0;
      capacity = capacity + (capacity >> 1);
      data = std::make_unique<storage_type[]>(capacity);
    }

    T* ptr = std::launder(new (&data[size++]) T(std::forward<Args>(args)...));
    return *ptr;
  }
};

UaContext ua;
TreeContext tree;
std::vector<char> bytes;
std::vector<char> loaded;
std::string filename = "files.out";

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

struct proto_node {
  struct {
    std::uint64_t start;
    std::uint64_t end;
  } sub;

  struct {
    std::uint64_t start;
    std::uint64_t end;
  } name;

  struct {
    std::uint64_t meta;
  } misc;

  static std::uint64_t make_id(int32_t findex, uint32_t jobid)
  {
    std::uint64_t low = 0;
    std::memcpy(&low, &findex, sizeof(findex));
    std::uint64_t high = jobid;
    return high << 32 | low;
  }
};

struct tree_header {
  std::uint64_t num_nodes;
  std::uint64_t nodes_offset;
  std::uint64_t string_offset;
  std::uint64_t meta_offset;
};

struct node_meta {
  int32_t findex{};           /* file index */
  uint32_t jobid{};           /* JobId */
  unsigned int type : 8;      /* node type */
  unsigned int hard_link : 1; /* set if have hard link */
  unsigned int soft_link : 1; /* set if is soft link */

  std::uint64_t delta_seq;
  std::uint64_t fhnode;
  std::uint64_t fhinfo;
  std::int64_t original;  // -1 = no original
};

class parts {
 public:
  parts(std::string_view path) : path{path} {}

  class iter {
   public:
    iter() : rest{""}, current{rest} {}

    iter(std::string_view path)
    {
      auto pos = path.find_first_of("/");
      current = path.substr(0, pos);
      rest = path.substr(std::min(pos + 1, path.size()));
    }

    friend bool operator==(const iter& left, const iter& right)
    {
      return left.rest == right.rest && left.current == right.current;
    }

    friend bool operator!=(const iter& left, const iter& right)
    {
      return !(left == right);
    }

    iter& operator++()
    {
      if (rest.size() > 0) {
        auto pos = rest.find_first_of("/");
        current = rest.substr(0, pos);
        rest.remove_prefix(std::min(pos + 1, rest.size()));
      } else {
        current = rest;
      }
      return *this;
    }

    std::string_view operator*() const { return current; }

    std::string_view path() const
    {
      auto* start = current.data();
      auto* end = rest.data() + rest.size();
      return std::string_view{start, static_cast<std::size_t>(end - start)};
    }

   private:
    std::string_view rest;
    std::string_view current;
  };

  iter begin() { return iter{path}; }

  static iter end() { return iter{}; }

 private:
  std::string_view path;
};

std::pair<std::size_t, std::string_view> get_base(std::string_view current,
                                                  std::string_view next)
{
  auto cur_parts = parts(current);
  auto next_parts = parts(next);

  auto c = cur_parts.begin();
  auto n = next_parts.begin();

  std::size_t common = 0;
  while (c != cur_parts.end() && n != next_parts.end()) {
    if (*c == *n) {
      common += 1;
      ++c;
      ++n;
    } else {
      break;
    }
  }

  return {common, n.path()};
}

struct tree {
  tree_header header{};
  span<proto_node> nodes{};
  // probably best to have one span per important meta field,
  // i.e. type, hardlink (offset of original hl file), id (= findex | jobid)
  // other stuff is not necessary.
  span<node_meta> metas{};
  std::string_view string_pool{};
  std::vector<bool> marked{};

  tree() {}
  tree(span<char> bytes)
  {
    std::memcpy(&header, bytes.data(), sizeof(header));

    auto count = header.num_nodes;
    {
      const proto_node* start
          = (const proto_node*)(bytes.data() + header.nodes_offset);
      nodes = span(start, count);
    }
    {
      const node_meta* start
          = (const node_meta*)(bytes.data() + header.meta_offset);
      metas = span(start, count);
    }
    {
      const char* start = (const char*)(bytes.data() + header.string_offset);
      std::size_t size = bytes.size() - header.string_offset;

      string_pool = std::string_view(start, size);
    }

    marked.resize(count);
  }

  std::size_t size() const { return nodes.size(); }

  bool match(std::string_view pattern, std::string_view str)
  {
    std::string p{pattern};
    std::string s{str};

    return fnmatch(p.c_str(), s.c_str(), 0) == 0;
  }

  std::size_t unmark(std::size_t start, std::size_t end)
  {
    std::fill(marked.begin() + start, marked.begin() + end, false);

    std::size_t count = 0;
    for (std::size_t i = start; i < end; ++i) {
      auto& meta = metas[i];
      count += (meta.type == TN_FILE);
    }
    return count;
  }
  std::size_t mark(std::size_t start, std::size_t end)
  {
    std::fill(marked.begin() + start, marked.begin() + end, true);
    // its _much_ faster without this

    std::size_t count = 0;
    for (std::size_t i = start; i < end; ++i) {
      auto& meta = metas[i];
      count += (meta.type == TN_FILE);

      if (meta.original >= 0) {
        count += (marked[meta.original] == false);
        marked[meta.original] = true;
      }
    }
    return count;
  }

  std::size_t unmark_glob(std::string_view glob,
                          std::size_t start,
                          std::size_t end)
  {
    std::size_t count = 0;

    auto part_end = glob.find_first_of('/');

    auto part = glob.substr(0, part_end);

    std::string_view rest{};
    if (part.size() != glob.size()) { rest = glob.substr(part_end + 1); }

    while (start < end) {
      const proto_node& current = nodes[start];
      const char* s_data = string_pool.data() + current.name.start;
      std::size_t s_size = current.name.end - current.name.start;
      std::string_view view{s_data, s_size};
      if (match(part, view)) {
        if (rest.size() == 0) {
          count += unmark(start, current.sub.end);
        } else {
          auto num = unmark_glob(rest, start, current.sub.end);
          if (num && !marked[start]) {
            marked[start] = true;
            num += 1;
          }
          count += num;
        }
      }

      start = current.sub.end;
    }

    return count;
  }

  std::size_t mark_glob(std::string_view glob,
                        std::size_t start,
                        std::size_t end)
  {
    std::size_t count = 0;

    auto part_end = glob.find_first_of('/');

    auto part = glob.substr(0, part_end);

    std::string_view rest{};
    if (part.size() != glob.size()) { rest = glob.substr(part_end + 1); }

    while (start < end) {
      const proto_node& current = nodes[start];
      const char* s_data = string_pool.data() + current.name.start;
      std::size_t s_size = current.name.end - current.name.start;
      std::string_view view{s_data, s_size};
      if (match(part, view)) {
        if (rest.size() == 0) {
          count += mark(start, current.sub.end);
        } else {
          auto num = mark_glob(rest, start, current.sub.end);
          if (num && !marked[start]) {
            marked[start] = true;
            num += 1;
          }
          count += num;
        }
      }

      start = current.sub.end;
    }

    return count;
  }

  std::size_t mark_glob(std::string_view glob)
  {
    if (glob.size() == 0) { return 0; }
    if (glob.back() == '/') { glob.remove_suffix(1); }
    return mark_glob(glob, 0, marked.size());
  }

  std::size_t unmark_glob(std::string_view glob)
  {
    if (glob.size() == 0) { return 0; }
    if (glob.back() == '/') { glob.remove_suffix(1); }
    return unmark_glob(glob, 0, marked.size());
  }

  std::string_view name_of(std::size_t idx)
  {
    const proto_node& current = nodes[idx];
    const char* s_data = string_pool.data() + current.name.start;
    std::size_t s_size = current.name.end - current.name.start;
    std::string_view view{s_data, s_size};
    return view;
  }
};

struct tree tree2;

struct tree_builder2 {
  // struct iter
  // {
  //   tree* t{nullptr};
  //   std::vector<uint32_t> stack{};

  //   iter(tree& t) : t{&t}
  // 		  , stack{0}
  //   {
  //   }

  //   void go_to(std::string_view path)
  //   {
  //     auto ps = parts(path);
  //     auto iter = ps.begin();

  //     std::size_t common = 0;
  //     while (common < stack.size() && iter != ps.end()) {
  // 	if (*iter == t->name_of(stack[common])) {
  // 	  ++common;
  // 	  ++iter;
  // 	} else {
  // 	  break;
  // 	}
  //     }

  //     stack.resize(common);

  //     if (stack.size() == 0) { return; }

  //     while (iter != ps.end()) {
  // 	auto last = stack.back();

  // 	auto& node = t->nodes[last];

  // 	auto child = node.sub.start;
  // 	auto end = node.sub.end;

  // 	bool found = false;
  // 	while (child < end) {
  // 	  if (t->name_of(child) == *iter) {
  // 	    stack.push_back(child);
  // 	    found = true;
  // 	    break;
  // 	  }

  // 	  child = t->nodes[child].sub.end + 1;
  // 	}

  // 	if (!found) {
  // 	  stack.clear();
  // 	  return;
  // 	}

  // 	++iter;
  //     }
  //   }
  // };
  std::vector<proto_node> nodes;
  std::vector<char> string_area;
  std::vector<node_meta> metas;

  std::pmr::monotonic_buffer_resource paths;
  std::pmr::monotonic_buffer_resource names;

  struct tree_data {
    int32_t findex;
    uint32_t jobid;

    struct stat s;
    int32_t link_findex;

    tree_data() {}
  };

  auto make_handler_ctx()
  {
    return std::make_tuple(array_list<std::pmr::string>(),
                           array_list<std::pmr::string>(),
                           std::vector<tree_data>());
  }

  static int insert_handler(void* ctx, int num_fields, char** row)
  {
    ASSERT(num_fields == 8);
    auto* arg = (decltype(tree_builder2{}.make_handler_ctx())*)ctx;

    tree_data& d = std::get<2>(*arg).emplace_back();
    std::get<0>(*arg).emplace_back(row[0]);
    std::get<1>(*arg).emplace_back(row[1]);
    // d.path = row[0];

    // d.fname.start = string_area = row[1];
    d.findex = str_to_int64(row[2]);
    d.jobid = str_to_int64(row[3]);
    DecodeStat(row[4], &d.s, sizeof(d.s), &d.link_findex);
    return 0;
  }

  // iter begin()
  // {
  //   return iter{this};
  // }
};

struct tree_builder {
  std::vector<proto_node> nodes;
  std::vector<char> string_area;
  std::vector<node_meta> metas;

  template <typename T> span<char> as_span(const T& val)
  {
    return span<char>(reinterpret_cast<const char*>(&val), sizeof(val));
  }

  using node_map = std::unordered_map<const TREE_NODE*, std::size_t>;

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
      node_meta& meta = metas.emplace_back();
      meta.findex = node->FileIndex;
      meta.jobid = node->JobId;
      meta.type = node->type;
      meta.hard_link = node->hard_link;
      meta.soft_link = node->soft_link;
      meta.delta_seq = node->delta_seq;
      meta.fhnode = node->fhnode;
      meta.fhinfo = node->fhinfo;
    }

    std::size_t start = nodes.size();

    TREE_NODE* child;
    foreach_child (child, const_cast<TREE_NODE*>(node)) { build(m, child); }

    std::size_t end = nodes.size();

    nodes[pos].sub.start = start;
    nodes[pos].sub.end = end;
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
      auto bytes = as_span(node);
      data.insert(data.end(), bytes.begin(), bytes.end());
    }

    auto string_offset = data.size();

    data.insert(data.end(), string_area.begin(), string_area.end());

    {
      std::size_t align = alignof(node_meta);
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
    // for (auto& meta : metas) {
    //   auto bytes = as_span(meta);
    //   data.insert(data.end(), bytes.begin(), bytes.end());
    // }

    tree_header header{nodes.size(), node_offset, string_offset, meta_offset};
    auto header_bytes = as_span(header);
    for (size_t i = 0; i < header_bytes.size(); ++i) {
      data[i] = header_bytes[i];
    }
    return data;
  }
};

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

TREE_ROOT* load_tree(struct tree tree)
{
  TREE_ROOT* root = new_tree(tree.size());

  TREE_NODE* nodes
      = (TREE_NODE*)RootMalloc(root, tree.size() * sizeof(TREE_NODE));

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
    for (std::size_t child = pnode.sub.start; child < pnode.sub.end;
         child = tree.nodes[child].sub.end + 1) {
      auto& childnode = nodes[child];
      node.child.insert(&childnode, NodeCompare);
      childnode.parent = &node;
    }
  }

  root->first = &nodes[0];
  root->last = &nodes[tree.size() - 1];
  root->last->next = nullptr;

  for (std::size_t i = 0; i < tree.size() - 1; ++i) {
    TREE_NODE& current = nodes[i];

    auto& meta = tree.metas[tree.nodes[i].misc.meta];
    current.next = &nodes[i + 1];
    current.FileIndex = meta.findex;
    current.JobId = meta.jobid;
    current.delta_seq = meta.delta_seq;
    current.delta_list = nullptr;  // ??
    current.fhinfo = meta.fhinfo;
    current.fhnode = meta.fhnode;

    auto str = tree.nodes[i].name;
    std::memcpy(current.fname, tree.string_pool.data() + str.start,
                str.end - str.start);
    current.fname[str.end - str.start] = 0;

    current.type = meta.type;
    current.extract = 0;
    current.extract_dir = 0;
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
  }

  return root;
}


const int max_depth = 30;

enum HIGH_FILE_NUMBERS
{
  hundred_thousand = 100'000,
  million = 1'000'000,
  ten_million = 10'000'000,
  hundred_million = 100'000'000,
  billion = 1'000'000'000
};

void InitContexts(UaContext* ua, TreeContext* tree, int count = 1)
{
  ua->cmd = GetPoolMemory(PM_FNAME);
  ua->args = GetPoolMemory(PM_FNAME);
  ua->verbose = true;
  ua->automount = true;
  ua->send = new OutputFormatter(sprintit, ua, filterit, ua);

  tree->root = new_tree(count);
  tree->ua = ua;
  tree->all = false;
  tree->FileEstimate = 100;
  tree->DeltaCount = 1;
  tree->node = (TREE_NODE*)tree->root;
}

int FakeCdCmd(UaContext* ua, TreeContext* tree, std::string path)
{
  std::string command = "cd " + path;
  PmStrcpy(ua->cmd, command.c_str());
  ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
  return cdcmd(ua, tree);
}

int FakeMarkCmd(UaContext* ua, TreeContext* tree, std::string path)
{
  std::string command = "mark " + path;
  PmStrcpy(ua->cmd, command.c_str());

  ParseArgsOnly(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
  return MarkElements(ua, tree);
}

void PopulateTree(int quantity, TreeContext* tree)
{
  me = new DirectorResource;
  me->optimize_for_size = true;
  me->optimize_for_speed = false;
  InitContexts(&ua, tree, quantity);

  char* filename = GetPoolMemory(PM_FNAME);
  char* path = GetPoolMemory(PM_FNAME);


  std::string file_path = "/";
  std::string file{};

  std::size_t nfile = 0;
  std::size_t ndir = 0;
  for (int i = 0; i < max_depth; ++i) {
    file_path.append("dir" + std::to_string(i) + "/");
    ndir += 1;
    for (int j = 0; j < (quantity / max_depth); ++j) {
      nfile += 1;
      file = "file" + std::to_string(j);

      strcpy(path, file_path.c_str());
      strcpy(filename, file.c_str());

      char* row0 = path;
      char* row1 = filename;
      char row2[] = "1";
      char row3[] = "2";
      char row4[]
          = "P0C BHoVZ IGk B Po Po A Cr BAA I BlA+1A BF2dbV BlA+1A A A C";
      char row5[] = "0";
      char row6[] = "0";
      char row7[] = "0";
      char* row[] = {row0, row1, row2, row3, row4, row5, row6, row7};

      InsertTreeHandler(tree, 0, row);
    }
  }
}

void PopulateTree2(int quantity)
{
  tree_builder2 builder;
  auto ctx = builder.make_handler_ctx();
  // std::get<0>(ctx).reserve(quantity);
  // std::get<1>(ctx).reserve(quantity);
  std::get<2>(ctx).reserve(quantity);

  char* filename = GetPoolMemory(PM_FNAME);
  char* path = GetPoolMemory(PM_FNAME);


  std::string file_path = "/";
  std::string file{};

  for (int i = 0; i < max_depth; ++i) {
    file_path.append("dir" + std::to_string(i) + "/");
    for (int j = 0; j < (quantity / max_depth); ++j) {
      file = "file" + std::to_string(j);

      strcpy(path, file_path.c_str());
      strcpy(filename, file.c_str());

      char* row0 = path;
      char* row1 = filename;
      char row2[] = "1";
      char row3[] = "2";
      char row4[]
          = "P0C BHoVZ IGk B Po Po A Cr BAA I BlA+1A BF2dbV BlA+1A A A C";
      char row5[] = "0";
      char row6[] = "0";
      char row7[] = "0";
      char* row[] = {row0, row1, row2, row3, row4, row5, row6, row7};

      tree_builder2::insert_handler((void*)&ctx, 8, row);
    }
  }

  // auto [paths, names, data] = std::move(ctx);

  // std::vector<uint32_t> idx;
  // std::cout << paths.size() << std::endl;
  // idx.resize(paths.size());

  // std::iota(idx.begin(), idx.end(), 0);

  // std::sort(idx.begin(), idx.end(), [&names, &paths](uint32_t a, uint32_t b)
  // {
  //   if (paths[a] < paths[b]) return true;
  //   return names[a] < names[b];
  // });
  // benchmark::DoNotOptimize(idx);

  benchmark::DoNotOptimize(ctx);
}

static void BM_populatetree(benchmark::State& state)
{
  for (auto _ : state) { PopulateTree(state.range(0), &tree); }
}

[[maybe_unused]] static void BM_populatetree2(benchmark::State& state)
{
  for (auto _ : state) { PopulateTree2(state.range(0)); }
}


[[maybe_unused]] static void BM_markallfiles(benchmark::State& state)
{
  FakeCdCmd(&ua, &tree, "/");
  [[maybe_unused]] int count = 0;
  for (auto _ : state) { count = FakeMarkCmd(&ua, &tree, "*"); }

  std::cout << "Marked: " << count << " files." << std::endl;
}

[[maybe_unused]] static void BM_buildtree(benchmark::State& state)
{
  for (auto _ : state) {
    tree_builder builder(tree.root);
    bytes = builder.to_bytes();
  }

  std::cout << bytes.size() << std::endl;
}

[[maybe_unused]] static void BM_savetree(benchmark::State& state)
{
  for (auto _ : state) {
    unlink(filename.c_str());
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT, S_IRWXU);
    ASSERT(fd >= 0);
    std::size_t written = 0;
    while (written < bytes.size()) {
      auto bytes_written
          = write(fd, (bytes.data() + written), bytes.size() - written);
      ASSERT(bytes_written > 0);

      written += bytes_written;
    }

    ASSERT(fsync(fd) == 0);
    ASSERT(close(fd) == 0);
  }
}

[[maybe_unused]] static void BM_loadtree(benchmark::State& state)
{
  for (auto _ : state) {
    int fd = open(filename.c_str(), O_RDONLY);
    ASSERT(fd >= 0);
    struct stat s;
    ASSERT(fstat(fd, &s) == 0);
    ASSERT(s.st_size == (ssize_t)bytes.size());
    std::size_t read = 0;
    while (read < bytes.size()) {
      auto bytes_read = ::read(fd, bytes.data() + read, bytes.size() - read);
      ASSERT(bytes_read > 0);

      read += bytes_read;
    }

    ASSERT(close(fd) == 0);
    tree2 = (struct tree)(span(bytes.data(), bytes.size()));
    TREE_ROOT* root = load_tree(tree2);

    state.PauseTiming();
    FreeTree(root);
    state.ResumeTiming();
    benchmark::DoNotOptimize(root);
  }
}

[[maybe_unused]] static void BM_markallfiles2(benchmark::State& state)
{
  [[maybe_unused]] std::size_t count = 0;
  for (auto _ : state) { count = tree2.mark_glob("*"); }

  std::cout << "Marked: " << count << " files." << std::endl;
}


// BENCHMARK(BM_populatetree)
//     ->Arg(HIGH_FILE_NUMBERS::hundred_thousand)
//     ->Unit(benchmark::kSecond);
// BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);
// BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
// BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);


// BENCHMARK(BM_populatetree)
//     ->Arg(HIGH_FILE_NUMBERS::million)
//     ->Unit(benchmark::kSecond);
// BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);
// BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
// BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);


// BENCHMARK(BM_populatetree)
//     ->Arg(HIGH_FILE_NUMBERS::ten_million)
//     ->Unit(benchmark::kSecond);
// BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);
// BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
// BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);

// BENCHMARK(BM_populatetree2)->Arg(100'000'000)->Unit(benchmark::kSecond);
BENCHMARK(BM_populatetree)->Arg(50'000'000)->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);

BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
BENCHMARK(BM_savetree)->Unit(benchmark::kSecond);
BENCHMARK(BM_loadtree)->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);


/*
 * Over ten million files requires quiet a bit a ram, so if you are going to
 * use the higher numbers, make sure you have enough ressources, otherwise the
 * benchmark will crash
 */

BENCHMARK_MAIN();
