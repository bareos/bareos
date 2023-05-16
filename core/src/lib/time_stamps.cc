#include <lib/time_stamps.h>

#include <mutex>
#include <sstream>

#include "include/baconfig.h"

ThreadTimeKeeper::ThreadTimeKeeper()
{}

void ThreadTimeKeeper::enter(const BlockIdentity& block)
{
  std::unique_lock _{vec_mut};
  // TODO: these have to be the same event!
  auto& event = stack.emplace_back(block);
  eventbuffer.emplace_back(event);
}

void ThreadTimeKeeper::switch_to(const BlockIdentity& block)
{
  std::unique_lock _{vec_mut};
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  eventbuffer.emplace_back(event);
  stack.back() = OpenEvent(block);
}

void ThreadTimeKeeper::exit()
{
  std::unique_lock _{vec_mut};
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  eventbuffer.emplace_back(event);
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
    for (auto& event : local.eventbuffer) {
      if (auto const* e = std::get_if<OpenEvent>(&event)) {
	gen->begin_event(thread_id, *e);
      } else if (auto* e = std::get_if<CloseEvent>(&event)) {
	gen->end_event(thread_id, *e);
      }
    }
    gen->end_thread(thread_id);
  }
  gen->end_report();
}
