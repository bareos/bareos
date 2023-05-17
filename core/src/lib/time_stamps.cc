#include <lib/time_stamps.h>

#include <mutex>
#include <sstream>

#include "include/baconfig.h"

ThreadTimeKeeper::~ThreadTimeKeeper()
{
  keeper.handle_event_buffer(std::move(buffer));
}

void ThreadTimeKeeper::enter(const BlockIdentity& block)
{
  std::unique_lock _{vec_mut};
  if (buffer.events.size() > 20000) {
    keeper.handle_event_buffer(std::move(buffer));
    buffer = EventBuffer(this_id, 20000, stack);
  }
  auto& event = stack.emplace_back(block);
  buffer.events.emplace_back(event);
}

void ThreadTimeKeeper::switch_to(const BlockIdentity& block)
{
  std::unique_lock _{vec_mut};
  if (buffer.events.size() > 20000) {
    keeper.handle_event_buffer(std::move(buffer));
    buffer = EventBuffer(this_id, 20000, stack);
  }
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  buffer.events.push_back(event);
  stack.back() = event::OpenEvent(block);
  buffer.events.push_back(stack.back());
}

void ThreadTimeKeeper::exit()
{
  std::unique_lock _{vec_mut};
  if (buffer.events.size() > 20000) {
    keeper.handle_event_buffer(std::move(buffer));
    buffer = EventBuffer(this_id, 20000, stack);
  }
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  buffer.events.push_back(event);
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
					   std::forward_as_tuple(*this));
    ASSERT(inserted);
    return iter->second;
  }
}

void TimeKeeper::add_writer(std::shared_ptr<ReportGenerator> gen)
{
  auto now = event::clock::now();
  gen->begin_report(now);
  {
    std::unique_lock lock{gen_mut};
    gens.emplace_back(std::move(gen));
  }
}

void TimeKeeper::remove_writer(std::shared_ptr<ReportGenerator> gen)
{
  {
    std::unique_lock lock{gen_mut};
    gens.erase(std::remove(gens.begin(), gens.end(), gen), gens.end());
  }
  auto now = event::clock::now();
  gen->end_report(now);
}
