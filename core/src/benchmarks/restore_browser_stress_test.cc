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


using namespace directordaemon;

UaContext ua;
TreeContext tree;
std::vector<char> bytes;

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
  std::uint64_t meta_offset;
  std::uint64_t string_offset;
};

struct node_meta {
  int32_t findex{};           /* file index */
  uint32_t jobid{};           /* JobId */
  unsigned int type : 8;      /* node type */
  unsigned int hard_link : 1; /* set if have hard link */
  unsigned int soft_link : 1; /* set if is soft link */
};

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

  bool match(std::string_view pattern, std::string_view str)
  {
    std::string p{pattern};
    std::string s{str};

    return fnmatch(p.c_str(), s.c_str(), 0) == 0;
  }

  std::size_t mark(std::size_t start, std::size_t end)
  {
    std::fill(marked.begin() + start, marked.begin() + end, true);
    // its _much_ faster without this
    return std::count_if(metas.begin() + start, metas.begin() + end,
                         [](const auto& x) {
                           // todo: somehow this is always false!
                           return x.type == TN_FILE;
                         });
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
};

struct tree tree2;

struct tree_builder2 {
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
    return std::make_tuple(std::pmr::vector<std::pmr::string>(&paths),
                           std::pmr::vector<std::pmr::string>(&names),
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
};

struct tree_builder {
  std::vector<proto_node> nodes;
  std::vector<char> string_area;
  std::vector<node_meta> metas;

  template <typename T> span<char> as_span(const T& val)
  {
    return span<char>(reinterpret_cast<const char*>(&val), sizeof(val));
  }

  tree_builder(const TREE_ROOT* root) { build(root->first); }

  void build(const TREE_NODE* node)
  {
    if (!node) { return; }
    std::size_t pos = nodes.size();
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
    }

    std::size_t start = nodes.size();

    TREE_NODE* child;
    foreach_child (child, const_cast<TREE_NODE*>(node)) { build(child); }

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
    for (auto& meta : metas) {
      auto bytes = as_span(meta);
      data.insert(data.end(), bytes.begin(), bytes.end());
    }

    tree_header header{nodes.size(), node_offset, string_offset, meta_offset};
    auto header_bytes = as_span(header);
    for (size_t i = 0; i < header_bytes.size(); ++i) {
      data[i] = header_bytes[i];
    }
    return data;
  }
};

const int max_depth = 30;

enum HIGH_FILE_NUMBERS
{
  hundred_thousand = 100'000,
  million = 1'000'000,
  ten_million = 10'000'000,
  hundred_million = 100'000'000,
  billion = 1'000'000'000
};

void InitContexts(UaContext* ua, TreeContext* tree)
{
  ua->cmd = GetPoolMemory(PM_FNAME);
  ua->args = GetPoolMemory(PM_FNAME);
  ua->verbose = true;
  ua->automount = true;
  ua->send = new OutputFormatter(sprintit, ua, filterit, ua);

  tree->root = new_tree(1);
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
  InitContexts(&ua, tree);

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

      InsertTreeHandler(tree, 0, row);
    }
  }
}

void PopulateTree2(int quantity)
{
  tree_builder2 builder;
  auto ctx = builder.make_handler_ctx();
  // ctx.first.reserve(quantity);
  // ctx.second.reserve(quantity);

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

  benchmark::DoNotOptimize(ctx);
}

static void BM_populatetree(benchmark::State& state)
{
  for (auto _ : state) { PopulateTree(state.range(0), &tree); }
}

// static void BM_populatetree2(benchmark::State& state)
// {
//   for (auto _ : state) { PopulateTree2(state.range(0)); }
// }

static void BM_buildtree(benchmark::State& state)
{
  for (auto _ : state) {
    tree_builder builder(tree.root);
    bytes = builder.to_bytes();
    tree2 = (struct tree)(span(bytes.data(), bytes.size()));
  }

  std::cout << bytes.size() << std::endl;
}

static void BM_markallfiles(benchmark::State& state)
{
  FakeCdCmd(&ua, &tree, "/");
  [[maybe_unused]] int count = 0;
  for (auto _ : state) { count = FakeMarkCmd(&ua, &tree, "*"); }

  std::cout << "Marked: " << count << " files." << std::endl;
}

static void BM_markallfiles2(benchmark::State& state)
{
  [[maybe_unused]] std::size_t count = 0;
  for (auto _ : state) { count = tree2.mark_glob("*"); }

  std::cout << "Marked: " << count << " files." << std::endl;
}

BENCHMARK(BM_populatetree)
    ->Arg(HIGH_FILE_NUMBERS::hundred_thousand)
    ->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);
BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);


BENCHMARK(BM_populatetree)
    ->Arg(HIGH_FILE_NUMBERS::million)
    ->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);
BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);


BENCHMARK(BM_populatetree)
    ->Arg(HIGH_FILE_NUMBERS::ten_million)
    ->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);
BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);

// BENCHMARK(BM_populatetree2)
//     ->Arg(100'000'000)
//     ->Unit(benchmark::kSecond);
BENCHMARK(BM_populatetree)->Arg(100'000'000)->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles)->Unit(benchmark::kSecond);
BENCHMARK(BM_buildtree)->Unit(benchmark::kSecond);
BENCHMARK(BM_markallfiles2)->Unit(benchmark::kSecond);

/*
 * Over ten million files requires quiet a bit a ram, so if you are going to
 * use the higher numbers, make sure you have enough ressources, otherwise the
 * benchmark will crash
 */

BENCHMARK_MAIN();
