#include "lib/perf_report.h"

#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstring>

#define ASSERT(...) (void) 0

struct SplitDuration {
  std::chrono::hours h;
  std::chrono::minutes m;
  std::chrono::seconds s;
  std::chrono::milliseconds ms;
  std::chrono::microseconds us;
  std::chrono::nanoseconds ns;

  template <typename Duration> SplitDuration(Duration d)
  {
    h = std::chrono::duration_cast<std::chrono::hours>(d);
    d -= h;
    m = std::chrono::duration_cast<std::chrono::minutes>(d);
    d -= m;
    s = std::chrono::duration_cast<std::chrono::seconds>(d);
    d -= s;
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);
    d -= ms;
    us = std::chrono::duration_cast<std::chrono::microseconds>(d);
    d -= us;
    ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
  }

  int64_t hours() { return h.count(); }
  int64_t minutes() { return m.count(); }
  int64_t seconds() { return s.count(); }
  int64_t millis() { return ms.count(); }
  int64_t micros() { return us.count(); }
  int64_t nanos() { return ns.count(); }

  friend std::ostream& operator<<(std::ostream& out, SplitDuration d)
  {
    return out << std::setfill('0')
	       << std::setw(2) << d.hours() << ":" << std::setw(2) << d.minutes() << ":" << std::setw(2) << d.seconds() << "."
	       << std::setw(3) << d.millis() << "-" << std::setw(3) << d.micros()
	       << std::setfill(' ');
  }
};

// static auto max_child_values(const ThreadCallstackReport::Node* node)
// {
//   struct { std::size_t name_length; std::size_t depth; } max;
//   max.depth = node->depth();
//   max.name_length = 0;
//   auto& children = node->children_view();
//   // this looks weird but is necessary for return type deduction
//   // since we want to call this function recursively
//   if (children.size() == 0) return max;
//   for (auto& [source, child] : children) {
//     auto child_max = max_child_values(child.get());
//     auto child_max_name = std::max(std::strlen(source->c_str()), child_max.name_length);
//     max.name_length = std::max(max.name_length, child_max_name);
//     max.depth = std::max(max.depth, child_max.depth);
//   }
//   return max;
// }

// static void PrintNode(std::ostringstream& out,
// 		      const char* name,
// 		      std::size_t depth,
// 		      std::chrono::nanoseconds parentns,
// 		      std::size_t max_name_length,
// 		      std::size_t max_depth,
// 		      const ThreadCallstackReport::Node* node)
// {
//     // depth is (modulo a shared offset) equal to current->depth
//   std::size_t offset = (max_name_length - std::strlen(name))
//     + (max_depth - depth);
//   SplitDuration d(node->time_spent());
//   out << std::setw(depth) << "" << name << ": " << std::setw(offset) << ""
//       << std::setfill('0')
//       << std::setw(2) << d.hours() << ":" << std::setw(2) << d.minutes() << ":" << std::setw(2) << d.seconds() << "."
//       << std::setw(3) << d.millis() << "-" << std::setw(3) << d.micros()
//       << std::setfill(' ');
//   if (parentns.count() != 0) {
//     out << " (" << std::setw(6) << std::fixed << std::setprecision(2) << double(node->time_spent().count() * 100) / double(parentns.count()) << "%%)";
//   }
//   out << "\n";

//   std::vector<std::pair<BlockIdentity const*, ThreadCallstackReport::Node const*>> children;
//   auto& view = node->children_view();
//   children.reserve(view.size());
//   for (auto& [source, child] : view) {
//     children.emplace_back(source, child.get());
//   }

//   std::sort(children.begin(), children.end(), [](auto& p1, auto& p2) {
//     auto t1 = p1.second->time_spent();
//     auto t2 = p2.second->time_spent();
//     if (t1 > t2) { return true; }
//     if ((t1 == t2) &&
// 	(p1.first > p2.first)) { return true; }
//     return false;
//   });
//   for (auto& [id, child] : children) {
//     PrintNode(out,
// 	      id->c_str(),
// 	      depth + 1,
// 	      node->time_spent(),
// 	      max_name_length,
// 	      max_depth,
// 	      child);
//   }
// }

// static std::int64_t PrintCollapsedNode(std::ostringstream& out,
// 				       std::string path,
// 				       const ThreadCallstackReport::Node* node)
// {
//   auto& view = node->children_view();

//   std::int64_t child_time = 0;
//   for (auto& [id, child] : view) {
//     std::string copy = path;
//     copy += ";";
//     copy += id->c_str();
//     child_time += PrintCollapsedNode(out,
// 				     copy,
// 				     child.get());
//   }

//   ASSERT(child_time <= node->time_spent().count());
//   out << path << " " << node->time_spent().count() - child_time << "\n";
//   out << path << " " << node->time_spent().count() << " | " << child_time  << "\n";
//   return node->time_spent().count();
// }

void ThreadOverviewReport::begin_report(event::time_point current) {
  std::unique_lock lock{mut};
  now = current;
}

void ThreadOverviewReport::begin_event(event::OpenEvent e) {
  std::unique_lock lock{mut};
  stack.push_back(e);
}

void ThreadOverviewReport::end_event(event::CloseEvent e) {
  using namespace std::chrono;

  std::unique_lock lock{mut};
  ASSERT(stack.size() > 0);
  ASSERT(stack.back().source == e.source);

  auto start = stack.back().start;
  auto end   = e.end;

  cul_time[e.source] += duration_cast<nanoseconds>(end - start);

  stack.pop_back();
}

std::unordered_map<BlockIdentity const*, std::chrono::nanoseconds> ThreadOverviewReport::as_of(event::time_point tp) const
{
  using namespace std::chrono;
  std::unordered_map<BlockIdentity const*, std::chrono::nanoseconds> result;

  {
    std::unique_lock lock{mut};
    result = cul_time; // copy
    for (auto& open : stack) {
      if (open.start > tp) continue;
      result[open.source] += duration_cast<nanoseconds>(tp - open.start);
    }
  }

  return result;
}

std::string OverviewReport::str() const
{
  using namespace std::chrono;
  std::ostringstream report{};
  event::time_point now = event::clock::now();
  report << "=== Start Performance Report (Overview) ===\n";
  std::unique_lock lock{threads_mut};
  for (auto& [id, reporter] : threads) {
    report << "== Thread: " << id << " ==\n";
    auto data = reporter.as_of(now);

    std::vector<std::pair<BlockIdentity const*, nanoseconds>> entries(data.begin(), data.end());
    std::sort(entries.begin(), entries.end(), [](auto& p1, auto& p2) {
      if (p1.second > p2.second) { return true; }
      if ((p1.second == p2.second) &&
	  (p1.first > p2.first)) { return true; }
      return false;
    });

    if (NumToShow != ShowAll) {
      entries.resize(NumToShow);
    }

    std::size_t maxwidth = 0;
    for (auto [id, _] : entries) {
      maxwidth = std::max(std::strlen(id->c_str()), maxwidth);
    }

    auto max_time = duration_cast<nanoseconds>(now - start);
    for (auto [id, time] : entries) {
      SplitDuration d(time);
      // TODO(C++20): replace this with std::format
      report << std::setw(maxwidth)
	     << id->c_str() << ": "
	     << d
	// XXX.XX = 6 chars
	     << " (" << std::setw(6) << std::fixed << std::setprecision(2) << double(time.count() * 100) / double(max_time.count()) << "%)"
	     << "\n";

    }
  }
  report << "=== End Performance Report ===\n";
  return report.str();
}
