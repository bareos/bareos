#include <lib/time_stamps.h>

#include <mutex>
#include <sstream>

#include "include/baconfig.h"

Event::Event(const BlockIdentity& block) : block{&block}
					 , start_point{std::chrono::steady_clock::now()}
{}

Event::time_point Event::end_point_as_of(Event::time_point current) const
{
  if (ended) {
    return end_point;
  } else {
    return current;
  }
}

void Event::end()
{
    ASSERT(!ended);
    ended = true;
    end_point = std::chrono::steady_clock::now();
}

ThreadTimeKeeper::ThreadTimeKeeper()
{}

void ThreadTimeKeeper::enter(const BlockIdentity& block)
{
  std::unique_lock _{vec_mut};
  events.emplace_back(block);
  stack.push_back(events.size() - 1);
}

void ThreadTimeKeeper::switch_to(const BlockIdentity& block)
{
  std::unique_lock _{vec_mut};
  ASSERT(stack.size() != 0);
  events[stack.back()].end();
  events.emplace_back(block);
  stack.back() = events.size() - 1;
}

void ThreadTimeKeeper::exit()
{
  std::unique_lock _{vec_mut};
  ASSERT(stack.size() != 0);
  events[stack.back()].end();
  stack.pop_back();
}

ThreadTimeKeeper& TimeKeeper::get_thread_local()
{
  // TODO: threadlocal here ?
  std::thread::id my_id = std::this_thread::get_id();
  {
    std::shared_lock read_lock(alloc_mut);
    if (auto found = keeper.find(my_id);
	found != keeper.end()) {
      return found->second;
    }
  }
  {
    std::unique_lock write_lock(alloc_mut);
    auto [iter, inserted] = keeper.emplace(std::piecewise_construct,
					   std::forward_as_tuple(my_id),
					   std::forward_as_tuple());
    ASSERT(inserted);
    return iter->second;
  }
}

void TimeKeeper::generate_report(ReportGenerator* gen) const
{
  auto current_time = std::chrono::steady_clock::now();

  std::unique_lock prevent_creations(alloc_mut);

  gen->begin_report(current_time);
  for (auto& [thread_id, local] : keeper) {
    gen->begin_thread(thread_id);
    std::unique_lock _{local.vec_mut};
    for (auto& event : local.events) {
      gen->add_event(event);
    }
    gen->end_thread();
  }
  gen->end_report();
}
