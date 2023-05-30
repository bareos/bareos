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

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/bsock.h"

#include <charconv>
#include <unordered_set>

namespace report {
struct words {
  class iterator {
  public:
    iterator(const char* start, const char* end, char sep)
      : start{start}
      , end{end}
      , word_end{start}
      , sep{sep}
    {
      find_word();
    }

    iterator(std::string_view view, char sep) : iterator{view.data(),
							 view.data() + view.size(),
							 sep}
    {
    }

    std::string_view operator*() const {
      using size_type = std::string_view::size_type;
      return std::string_view{start, static_cast<size_type>(word_end - start)};
    }

    iterator& operator++() {
      start = word_end;
      find_word();
      return *this;
    }

    bool operator==(const iterator& iter) {
      // no need to compare word_end as well
      return iter.start == start &&
	iter.end == end &&
	iter.sep == sep;
    }
    bool operator!=(const iterator& iter) {
      return !operator==(iter);
    }

  private:
    const char* start;
    const char* end;
    const char* word_end;
    char sep;

    void find_word() {
        start = std::find_if_not(start, end, [sep = this->sep](char c) { return c == sep; });
        word_end = std::find_if(start, end, [sep = this->sep](char c) { return c == sep; });
    }
  };

  words(std::string_view v,
	char sep = ' ') : view{v}
			, sep{sep}
  {
  }

  iterator begin() const {
    return iterator{view, sep};
  }

  iterator end() const {
    return iterator{view.substr(view.size()), sep};
  }

  std::string_view view;
  char sep;
};

// result is only guaranteed to live as long as str.
std::unordered_map<std::string_view, std::string_view> ParseReportCommands(std::string_view str)
{
  std::unordered_map<std::string_view, std::string_view> default_keys {
    {"about", "perf"},
    {"jobid", "all"},
    {"style", "callstack"},
    {"depth", "all"},
    {"show", "all"},
    {"relative", "default"}
  };

  std::unordered_map<std::string_view, std::string_view> result;

  bool skipped_first = false;
  for (auto word : words(str)) {
    if (skipped_first) {
      // word can either be a key-value pair of the form key=value
      // or just a simple flag.  In that case we insert a "yes"

      auto npos = std::string_view::npos;
      if (auto eqpos = word.find_first_of('='); eqpos != npos) {
	// kv pair
	result.emplace(word.substr(0, eqpos),
		       word.substr(eqpos+1));
      } else {
	// flag
	result.emplace(word, "yes");
      }
    } else {
      // skip 'report'
      skipped_first = true;
    }
  }

  result.merge(default_keys);
  return result;
}

struct callstack_options
{
  std::size_t max_depth;
  bool relative;
  std::string to_string(const PerformanceReport& callstack) {
    return callstack.callstack_str(max_depth, relative);
  }
};

struct collapsed_options
{
  std::size_t max_depth;
  std::string to_string(const PerformanceReport& callstack) {
      return callstack.collapsed_str(max_depth);
  }
};

struct overview_options
{
  std::size_t top_n;
  bool relative;
  std::string to_string(const PerformanceReport& callstack) {
    return callstack.overview_str(top_n, relative);
  }
};

template<typename T>
bool ParseInt(BareosSocket* dir,
	      const char* start,
	      const char* end,
	      T& val)
{
  auto result = std::from_chars(start,
				end,
				val);
  if (result.ec != std::errc() ||
      result.ptr != end) {
    std::string error_str(end - start, '~');
    std::size_t problem = result.ptr - start;
    error_str[problem] = '^';
    for (std::size_t i = 0; i < problem; ++i) {
      error_str[i] = ' ';
    }
    dir->fsend("Could not parse number value: %s\n"
	       "                              %s\n",
	       std::string{start, end}.c_str(),
	       error_str.c_str());
    return false;
  }
  return true;
}
std::optional<overview_options> ParseOverviewOptions(BareosSocket* dir,
						     const std::unordered_map<std::string_view, std::string_view>& map)
{
  overview_options options;
  if (auto found = map.find("show");
      found != map.end()) {
    auto val = found->second;
    if (val == "all") {
      options.top_n = PerformanceReport::ShowAll;
    } else {
      if (!ParseInt(dir, val.data(), val.data() + val.size(), options.top_n)) {
	return std::nullopt;
      }
    }
  }
  if (auto found = map.find("relative");
      found != map.end()) {
    auto val = found->second;
    if (val == "default") {
      options.relative = false;
    } else if (val == "yes") {
      options.relative = true;
    } else if (val == "no") {
      options.relative = false;
    } else {
      dir->fsend("Could not parse boolean 'relative': %s\n", std::string{val}.c_str());
      return std::nullopt;
    }
  }
  return options;
}

std::optional<callstack_options> ParseCallstackOptions(BareosSocket* dir,
						       const std::unordered_map<std::string_view, std::string_view>& map)
{
  callstack_options options;
  if (auto found = map.find("depth");
      found != map.end()) {
    auto val = found->second;
    if (val == "all") {
      options.max_depth = PerformanceReport::ShowAll;
    } else {
      if (!ParseInt(dir, val.data(), val.data() + val.size(), options.max_depth)) {
	return std::nullopt;
      }
    }
  }
  if (auto found = map.find("relative");
      found != map.end()) {
    auto val = found->second;
    if (val == "default") {
      options.relative = true;
    } else if (val == "yes") {
      options.relative = true;
    } else if (val == "no") {
      options.relative = false;
    } else {
      dir->fsend("Could not parse boolean 'relative': %s\n", std::string{val}.c_str());
      return std::nullopt;
    }
  }
  return options;
}

std::optional<collapsed_options> ParseCollapsedOptions(BareosSocket* dir,
						       const std::unordered_map<std::string_view, std::string_view>& map)
{
  collapsed_options options;
  if (auto found = map.find("depth");
      found != map.end()) {
    auto val = found->second;
    if (val == "all") {
      options.max_depth = PerformanceReport::ShowAll;
    } else {
      if (!ParseInt(dir, val.data(), val.data() + val.size(), options.max_depth)) {
	return std::nullopt;
      }
    }
  }
  return options;
}

static bool PerformanceReport(BareosSocket* dir,
			      const std::unordered_map<std::string_view,
			      std::string_view>& options)
{
  std::variant<std::monostate, callstack_options, overview_options,
	       collapsed_options> parsed;

  bool all_jobids = false;
  std::unordered_set<std::uint32_t> jobids{};
  if (auto found = options.find("jobid");
      found != options.end()) {
    auto view = found->second;
    if (view == "all") {
      all_jobids = true;
    } else {
      for (auto num : words(view, ',')) {
	std::uint32_t jobid;
	if (ParseInt(dir, num.data(),
		     num.data() + num.size(),
		     jobid)) {
	  jobids.insert(jobid);
	} else {
	  return false;
	}
      }
    }
  } else {
    // this should never happen as jobid is set by default
    dir->fsend("Required field 'jobid' not supplied.\n");
    return false;
  }
  if (auto found = options.find("style");
      found != options.end()) {
    auto view = found->second;
    if (view == "callstack") {
      if (auto opt = ParseCallstackOptions(dir, options);
	  opt.has_value()) {
	parsed = opt.value();
      } else {
	return false;
      }
    } else if (view == "overview") {
      if (auto opt = ParseOverviewOptions(dir, options);
	  opt.has_value()) {
	parsed = opt.value();
      } else {
	return false;
      }
    } else if (view == "collapsed") {
      if (auto opt = ParseCollapsedOptions(dir, options);
	  opt.has_value()) {
	parsed = opt.value();
      } else {
	return false;
      }
    } else {
      dir->fsend("Perf Report: Unknown style '%s'.\n", std::string{view}.c_str());
      return false;
    }
  } else {
    // should not happen as style is set by default
    dir->fsend("Perf Report: No style was specified.\n");
    return false;
  }

  JobControlRecord* njcr;
  std::size_t NumJobs = 0;
  dir->fsend("Starting Report of running jobs.\n");
  foreach_jcr (njcr) {
    auto* perf_report = njcr->performance_report();
    if (njcr->JobId > 0) {
      if (!all_jobids && jobids.find(njcr->JobId) == jobids.end()) {
	continue;
      }
      dir->fsend(_("==== Job %d ====\n"), njcr->JobId);
      if (perf_report) {
	std::visit([dir, &perf_report](auto&& arg) {
	  using T = std::decay_t<decltype(arg)>;
	  if constexpr (std::is_same_v<T, callstack_options>) {
	    std::string str = arg.to_string(*perf_report);
	    dir->send(str.c_str(), str.size());
	  } else if constexpr (std::is_same_v<T, overview_options>) {
	    std::string str = arg.to_string(*perf_report);
	    dir->send(str.c_str(), str.size());
	  } else if constexpr (std::is_same_v<T, collapsed_options>) {
	    std::string str = arg.to_string(*perf_report);
	    dir->send(str.c_str(), str.size());
	  }
	}, parsed);
      } else {
	dir->fsend(_("Performance counters are disabled for this job.\n"));
      }
      dir->fsend(_("====\n"));
      NumJobs += 1;
    }
  }

  if (NumJobs) {
    dir->fsend("Reported on %lu job%s.\n", NumJobs,
		NumJobs > 1 ? "s" : "");
  } else {
    dir->fsend("There are no running jobs to report on.\n");
  }
  return true;
}
// Report command from Director
bool ReportCmd(JobControlRecord* jcr) {
  BareosSocket* dir = jcr->dir_bsock;

  std::string received{dir->msg};
  std::unordered_map parsed = ParseReportCommands(received);
  dir->fsend("Received: '%s'\n", received.c_str());

  for (auto [key, val] : parsed) {
    std::string k{key};
    std::string v{val};

    dir->fsend("%s -> %s\n", k.c_str(), v.c_str());
  }

  bool result = false;

  if (auto found = parsed.find("about");
      found != parsed.end()) {
    if (found->second == "perf") {
      result = PerformanceReport(dir, parsed);
    } else {
      // the map does not contain cstrings but string_views.  As such
      // we need to create a string first if we want to print them with %s.
      std::string s{found->second};
      dir->fsend(_("2900 Bad report command; unknown report %s.\n"),
		 s.c_str());
      result = false;
    }
  } else {
    // since about -> perf is the default, this should never happen
    dir->fsend(_("2900 Bad report command; no report type selected.\n"));
    result = false;
  }

  dir->signal(BNET_EOD);
  return result;
}
};
