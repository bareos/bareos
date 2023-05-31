#include <lib/time_stamps.h>

#include <mutex>
#include <sstream>

#include "include/baconfig.h"

Duration::Duration(std::string_view name) : name{name}
					  , start_point{std::chrono::steady_clock::now()}
{}

Duration::time_point Duration::end_point_as_of(Duration::time_point current) const
{
  if (ended) {
    return end_point;
  } else {
    return current;
  }
}

void Duration::end()
{
    ASSERT(!ended);
    ended = true;
    end_point = std::chrono::steady_clock::now();
}

ThreadTimeKeeper::ThreadTimeKeeper() : current{times.end()}
{
  enter("Thread");
}

void ThreadTimeKeeper::enter(std::string_view s)
{
  std::unique_lock _{vec_mut};
  current = times.emplace(times.end(), s);
}

void ThreadTimeKeeper::exit()
{
  std::unique_lock _{vec_mut};
  ASSERT(current != times.end());
  current->end();

  while (current != times.begin()) {
    --current;
    if (!current->ended) {
      break;
    }
  }

  if (current->ended) {
    current = times.end();
  }
}

TimedBlock::TimedBlock(ThreadTimeKeeper& keeper, std::string_view name) : keeper{keeper}
{
  keeper.enter(name);
}

TimedBlock::~TimedBlock()
{
  keeper.exit();
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

static std::uint64_t AsNS(Duration::time_point tp)
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
}

static void GenerateTimingReport(std::ostringstream& report,
			  std::chrono::steady_clock::time_point current,
			  const std::vector<Duration>& durations)
{
  for (auto& dur : durations) {
    Duration::time_point start = dur.start_point;
    Duration::time_point end   = dur.end_point_as_of(current);
    report << dur.name << ": " << AsNS(start) << " -- " << AsNS(end);
    if (!dur.ended) {
      report << " (still active)";
    }
    report << "\n";
  }
}

std::string TimeKeeper::generate_report() const
{
  auto current_time = std::chrono::steady_clock::now();

  std::unique_lock prevent_creations(alloc_mut);
  std::ostringstream report;
  report << "=== Performance Report ===\n";

  for (auto& [thread_id, local] : keeper) {
    report << "=== Thread: " << thread_id << " ===\n";
    std::unique_lock _{local.vec_mut};
    GenerateTimingReport(report, current_time, local.times);
  }

  return report.str();
}
