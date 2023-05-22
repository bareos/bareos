/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_PERF_REPORT_H_
#define BAREOS_LIB_PERF_REPORT_H_

#include <unordered_map>
#include <string>
#include <cstdint>

#include "lib/event.h"

enum class PerfReport : std::int32_t {
  Overview = (1 << 0),
  Stack    = (1 << 1),
};


class ReportGenerator {
public:
  virtual void begin_report(event::time_point start [[maybe_unused]]) {};
  virtual void end_report(event::time_point end [[maybe_unused]]) {};
  virtual void add_events(const EventBuffer& buf [[maybe_unused]]) {}
};

class ThreadOverviewReport {
 public:
  void begin_report(event::time_point current);
  void begin_event(event::OpenEvent e);
  void end_event(event::CloseEvent e);
  std::unordered_map<BlockIdentity const*, std::chrono::nanoseconds> as_of(event::time_point tp) const;
private:
  mutable std::mutex mut{};
  std::vector<event::OpenEvent> stack{};
  event::time_point now;
  std::unordered_map<BlockIdentity const*, std::chrono::nanoseconds> cul_time;
};

class OverviewReport : public ReportGenerator {
 public:
  static constexpr std::int32_t ShowAll = -1;

  OverviewReport(std::int32_t ShowTopN) : NumToShow{ShowTopN}
  {}

  void begin_report(event::time_point now) override
  {
    start = now;
  }

  void end_report(event::time_point now) override
  {
    end = now;
  }

  void add_events(const EventBuffer& buf) override
  {
    std::unique_lock lock{threads_mut};
    auto [iter, inserted] = threads.try_emplace(buf.threadid());
    auto& thread = iter->second;

    if (inserted) {
      thread.begin_report(start);
      for (auto& open : buf.stack()) {
	thread.begin_event(open);
      }
    }

    for (auto event : buf.events) {
      if (auto* open = std::get_if<event::OpenEvent>(&event)) {
	thread.begin_event(*open);
      } else if (auto* close = std::get_if<event::CloseEvent>(&event)) {
	thread.end_event(*close);
      }
    }
  }

  std::string str() const;

 private:
  std::int32_t NumToShow;
  event::time_point start, end;
  mutable std::mutex threads_mut{};
  std::unordered_map<std::thread::id, ThreadOverviewReport> threads{};
};

/* class ThreadCallstackReport { */
/* public: */
/*   class Node { */
/*   public: */
/*     using childmap = std::unordered_map<BlockIdentity const*, std::unique_ptr<Node>>; */
/*     Node* parent() const { return parent_; } */
/*     Node* child(BlockIdentity const* source) { */
/*       std::unique_ptr child = std::make_unique<Node>(this); */
/*       auto [iter, _] = children.try_emplace(source, std::move(child)); */

/*       return iter->second.get(); */
/*     } */
/*     bool is_open() const { */
/*       return since.has_value(); */
/*     } */
/*     void open(event::time_point at) { */
/*       ASSERT(!is_open()); */
/*       since = at; */
/*     } */
/*     void close(event::time_point at) { */
/*       using namespace std::chrono; */

/*       ASSERT(is_open()); */
/*       ns += duration_cast<nanoseconds>(at - since.value()); */
/*       since.reset(); */
/*     } */
/*     // creates a deep copy of this node, except every open node */
/*     // gets replaced by a closed node in the copy with the endtime being at */
/*     std::unique_ptr<Node> closed_deep_copy_at(event::time_point at) const { */
/*       using namespace std::chrono; */
/*       std::unique_ptr copy = std::make_unique<Node>(); */
/*       ASSERT(!is_open() || parent_ == nullptr); */
/*       copy->ns = ns; */
/*       copy->depth_ = depth_; */
/*       if (is_open() && at > since.value()) { */
/* 	copy->ns += duration_cast<nanoseconds>(at - since.value()); */
/*       } */

/*       for (auto& [source, child] : children) { */
/* 	auto [child_copy, inserted] = copy->children.emplace(source, child->closed_deep_copy_at(at)); */
/* 	ASSERT(inserted); */
/* 	child_copy->second->parent_ = copy.get(); */
/*       } */
/*       return copy; */
/*     } */

/*     std::chrono::nanoseconds time_spent() const { */
/*       // this should only be called on closed nodes! */
/*       return ns; */
/*     } */

/*     std::int32_t depth() const { return depth_; } */
/*     Node() = default; */
/*     Node(Node* parent) : parent_{parent} */
/* 		       , depth_{parent_->depth_ + 1} */
/*     {} */
/*     Node(const Node&) = default; */
/*     Node(Node&&) = default; */
/*     Node& operator=(Node&&) = default; */
/*     Node& operator=(const Node&) = default; */
/*     const childmap& children_view() const { return children; } */
/*   private: */
/*     std::optional<event::time_point> since{std::nullopt}; */
/*     Node* parent_{nullptr}; */
/*     std::int32_t depth_{0}; */
/*     std::chrono::nanoseconds ns{0}; */
/*     childmap children{}; */
/*   }; */

/*   void begin_report(event::time_point now) */
/*   { */
/*     top.open(now); */
/*   } */

/*   void begin_event(event::OpenEvent e) */
/*   { */
/*     std::unique_lock lock{node_mut}; */
/*     if (skipped_depth > 0) { */
/*       skipped_depth += 1; */
/*     } else if (current->depth() == max_depth) { */
/*       skipped_depth = 1; */
/*     } else { */
/*       current = current->child(e.source); */
/*       current->open(e.start); */
/*     } */
/*   } */

/*   void end_event(event::CloseEvent e) */
/*   { */
/*     std::unique_lock lock{node_mut}; */
/*     if (skipped_depth > 0) { */
/*       skipped_depth -= 1; */
/*     } else { */
/*       current->close(e.end); */
/*       current = current->parent(); */
/*       ASSERT(current != nullptr); */
/*     } */
/*   } */

/*   ThreadCallstackReport(std::int32_t max_depth) : max_depth{max_depth} */
/* 						, current{&top} */
/*   {} */
/*   static constexpr std::int32_t ShowAll = std::numeric_limits<std::int32_t>::max(); */

/*   std::unique_ptr<Node> as_of(event::time_point at) const { */
/*     std::unique_lock lock{node_mut}; */
/*     return top.closed_deep_copy_at(at); */
/*   } */
/* private: */
/*   std::int32_t skipped_depth{0}; */
/*   std::int32_t max_depth; */
/*   mutable std::mutex node_mut{}; */
/*   Node top{}; */
/*   Node* current{nullptr}; */
/* }; */


/* class CallstackReport : public ReportGenerator { */
/* public: */
/*   virtual void begin_report(event::time_point now) override { */
/*     start = now; */
/*   } */
/*   virtual void end_report(event::time_point now) override { */
/*     end = now; */
/*   } */

/*   virtual void add_events(const EventBuffer& buf) override { */
/*     ThreadCallstackReport* thread = nullptr; */
/*     auto thread_id = buf.threadid(); */
/*     std::shared_lock lock{threads_mut}; */
/*     bool inserted = false; */
/*     { */
/*       auto iter = threads.find(thread_id); */
/*       if (iter != threads.end()) { */
/* 	thread = &iter->second; */
/*       } */
/*     } */
/*     if (thread == nullptr) { */
/*       lock.unlock(); */
/*       std::unique_lock write_lock{threads_mut}; */
/*       auto [_, did_insert] = threads.try_emplace(thread_id, max_depth); */
/*       ASSERT(did_insert); */
/*       inserted = did_insert; */
/*       write_lock.unlock(); */
/*       lock.lock(); */
/*       // since we unlock and then lock something couldve happened */
/*       // that rehashes the map; as such we need to search again and cannot */
/*       // use the result of emplace */
/*       auto iter = threads.find(thread_id); */
/*       ASSERT(iter != threads.end()); */
/*       thread = &iter->second; */
/*     } */
/*     ASSERT(thread != nullptr); */

/*     if (inserted) { */
/*       thread->begin_report(start); */
/*       for (auto& open : buf.stack()) { */
/* 	thread->begin_event(open); */
/*       } */
/*     } */

/*     for (auto event : buf.events) { */
/*       if (auto* open = std::get_if<event::OpenEvent>(&event)) { */
/* 	thread->begin_event(*open); */
/*       } else if (auto* close = std::get_if<event::CloseEvent>(&event)) { */
/* 	thread->end_event(*close); */
/*       } */
/*     } */
/*   } */

/*   CallstackReport(std::int32_t max_depth) : max_depth{max_depth} {} */
/*   static constexpr std::int32_t ShowAll = ThreadCallstackReport::ShowAll; */
/*   std::string str() const { */
/*     using namespace std::chrono; */
/*     event::time_point now = event::clock::now(); */
/*     std::ostringstream report{}; */
/*     report << "=== Start Performance Report (Callstack) ===\n"; */
/*     std::shared_lock lock{threads_mut}; */
/*     for (auto& [id, thread] : threads) { */
/*       report << "== Thread: " << id << " ==\n"; */
/*       auto node = thread.as_of(now); */
/*       auto max_values = max_child_values(node.get()); */

/*       PrintNode(report, "Thread", */
/* 		0, */
/* 		duration_cast<nanoseconds>(now - start), */
/* 		std::max(std::size_t{6}, max_values.name_length), */
/* 		max_values.depth, */
/* 		node.get()); */
/*     } */
/*     report << "=== End Performance Report ===\n"; */
/*     return report.str(); */
/*   } */

/*   std::string collapsed_str() const { */
/*     using namespace std::chrono; */
/*     event::time_point now = event::clock::now(); */
/*     std::ostringstream report{}; */
/*     report << "=== Start Performance Report (Collapsed Callstack) ===\n"; */
/*     std::shared_lock lock{threads_mut}; */
/*     for (auto& [id, thread] : threads) { */
/*       report << "== Thread: " << id << " ==\n"; */
/*       auto node = thread.as_of(now); */
/*       PrintCollapsedNode(report, "Thread", */
/* 			 node.get()); */
/*     } */
/*     report << "=== End Performance Report ===\n"; */
/*     return report.str(); */
/*   } */
/* private: */
/*   std::int32_t max_depth; */
/*   event::time_point start, end; */
/*   mutable std::shared_mutex threads_mut{}; */
/*   std::unordered_map<std::thread::id, ThreadCallstackReport> threads{}; */
/* }; */

#endif /* BAREOS_LIB_PERF_REPORT_H_ */
