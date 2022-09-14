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

#include "lib/bnet_network_dump_private.h"
#include "cli.h"
#include "lib/bnet_network_dump.h"
#include "lib/version.h"
#include "lib/message.h"
#include "lib/edit.h"
#include <regex>

class BareosCliFormatter : public CLI::Formatter {
 public:
  std::string make_option_opts(const CLI::Option* opt) const override
  {
    std::stringstream out;

    if (!opt->get_option_text().empty()) {
      out << " " << opt->get_option_text();
    } else {
      if (opt->get_type_size() != 0) {
        if (!opt->get_type_name().empty()) {
          out << " " << get_label(opt->get_type_name());
        }
        if (opt->get_expected_max() == CLI::detail::expected_max_vector_size) {
          out << " ...";
        } else if (opt->get_expected_min() > 1) {
          out << " x " << opt->get_expected();
        }
        if (!opt->get_default_str().empty()) {
          out << "\n" << indent << indent;
          out << "Default: " << opt->get_default_str();
        }
        if (opt->get_required()) {
          out << "\n" << indent << indent;
          out << get_label("REQUIRED");
        }
      }
      if (!opt->get_envname().empty()) {
        out << "\n" << indent << indent;
        out << get_label("Env") << ": " << opt->get_envname();
      }
      if (!opt->get_needs().empty()) {
        out << "\n" << indent << indent;
        out << get_label("Needs") << ":";
        for (const CLI::Option* op : opt->get_needs())
          out << " " << op->get_name();
      }
      if (!opt->get_excludes().empty()) {
        out << "\n" << indent << indent;
        out << get_label("Excludes") << ":";
        for (const CLI::Option* op : opt->get_excludes())
          out << " " << op->get_name();
      }
    }
    return out.str();
  }

  std::string make_option(const CLI::Option* opt,
                          bool is_positional) const override
  {
    std::stringstream out;

    std::string name = make_option_name(opt, is_positional);
    // remove option values from string, eg.
    //    -s{false},--no-signals{false}
    // => -s,--no-signals
    name = std::regex_replace(name, std::regex("\\{[^}]*\\}"), "");
    out << indent << name;

    out << make_option_opts(opt);
    out << std::endl;

    std::string description = make_option_desc(opt);
    if (!description.empty()) {
      format_paragraph(out, description, indent + indent);
    }
    out << std::endl;

    return out.str();
  }

 protected:
  std::string indent = std::string("    ");
  std::size_t max_line_length = 79;

  std::ostream& format_paragraph(std::ostream& out,
                                 const std::string& text,
                                 const std::string& indent) const
  {
    std::istringstream text_iss(text);

    std::string word;
    unsigned characters_written = indent.size();

    out << indent;
    while (text_iss >> word) {
      if (word.size() + characters_written > max_line_length) {
        out << "\n";
        out << indent;
        characters_written = indent.size();
      }
      out << word << " ";
      characters_written += word.size() + 1;
    }
    out << std::endl;
    return out;
  }
};

void InitCLIApp(CLI::App& app, std::string description, int fsfyear)
{
  if (fsfyear) {
    std::vector<char> copyright(1024);
    kBareosVersionStrings.FormatCopyrightWithFsfAndPlanets(
        copyright.data(), copyright.size(), fsfyear);

    description += "\n" + std::string(copyright.data());
  }

  app.description(description);
  app.set_help_flag("-h,--help,-?", "Print this help message and exit.");
  app.set_version_flag("--version", kBareosVersionStrings.Full);
  app.formatter(std::make_shared<BareosCliFormatter>());
#ifdef HAVE_WIN32
  app.allow_windows_style_options();
#endif
  app.failure_message(CLI::FailureMessage::help);
}

void AddDeprecatedExportOptionsHelp(CLI::App& app)
{
  app.add_option(
         "-x",
         [&app]([[maybe_unused]] std::vector<std::string> vals) {
           app.failure_message(CLI::FailureMessage::simple);
           throw CLI::ParseError(
               "The -xc and -xs options have changed.\n"
               "Use --xc and --xs as given in the help.",
               CLI::ExitCodes::OptionNotFound);
           return false;
         },
         "For deprecated -xs and -xc flags.")
      ->group("");
}

void AddDebugOptions(CLI::App& app)
{
  app.add_option(
         "-d,--debug-level",
         [&app](std::vector<std::string> vals) {
           if (Is_a_number(vals.front().c_str())) {
             debug_level = stoi(vals.front());
             return true;
           } else if (vals.front() == "t") {
             app.failure_message(CLI::FailureMessage::simple);
             throw CLI::ParseError(
                 "The -dt option has changed.\n"
                 "Use --dt as given in the help.",
                 CLI::ExitCodes::OptionNotFound);
           }
           return false;
         },
         "Set debug level to <level>.")
      ->take_all()
      ->type_name("<level>");

  app.add_flag("--dt,--debug-timestamps", dbg_timestamp,
               "Print timestamps in debug output.");
}

void AddVerboseOption(CLI::App& app)
{
  app.add_flag("-v,--verbose", verbose, "Verbose user messages.");
}

void AddNetworkDebuggingOption(CLI::App& app)
{
  app.add_flag("--zp,--plantuml-mode", BnetDumpPrivate::plantuml_mode_,
               "Activate plant UML.")
      ->group("");  // add it to empty group to hide the option from help

  app.add_option("--zs,--set-dump-stack-level-amount",
                 BnetDumpPrivate::stack_level_amount_,
                 "Set stack level amount.")
      ->group("");  // add it to empty group to hide the option from help

  app.add_option("--zf,--set-dump-filename", BnetDumpPrivate::filename_,
                 "Set file name.")
      ->group("");  // add it to empty group to hide the option from help
}

void AddUserAndGroupOptions(CLI::App& app,
                            std::string& user,
                            std::string& group)
{
  app.add_option("-u,--user", user,
                 "Run as given user (requires starting as root)")
      ->type_name("<user>");
  app.add_option("-g,--group", group,
                 "Run as given group (requires starting as root)")
      ->type_name("<group>");
}
