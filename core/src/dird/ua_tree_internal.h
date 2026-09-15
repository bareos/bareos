/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_DIRD_UA_TREE_INTERNAL_H_
#define BAREOS_DIRD_UA_TREE_INTERNAL_H_

/**
 * @file
 * Declarations shared between the classic line-mode restore file selection
 * shell (ua_tree.cc) and the full-screen interactive tree browser
 * (ua_tree_browser.cc), so both can drive the exact same underlying
 * mark/unmark and command-dispatch logic instead of duplicating it.
 */

struct tree_node;

namespace directordaemon {

class UaContext;
struct TreeContext;

/**
 * Set/clear the "extract" (mark) flag on node, recursing into directories
 * and following hardlinks exactly like the classic mark/unmark commands.
 * Returns the number of nodes whose mark state actually changed.
 */
int SetExtract(UaContext* ua, tree_node* node, TreeContext* tree, bool extract);

// Outcome of running exactly one classic ("$ ") tree selection command.
enum class ClassicCommandOutcome
{
  kContinue,        /**< Stay in the classic command loop. */
  kLeaveSelection,  /**< "done"/"exit"/"quit"/"abort" or EOF: leave file
                     *  selection entirely (same as today). */
  kSwitchToBrowser, /**< The "browse" command was used: switch (back) into
                     *  the full-screen interactive tree browser. */
};

/**
 * Read and dispatch exactly one classic tree-selection command from the
 * user (the same "$ " prompt used since the classic mode has always used).
 * Shared by the persistent classic command loop and by the interactive
 * browser's ":" one-shot command escape.
 */
ClassicCommandOutcome RunOneClassicTreeCommand(UaContext* ua,
                                               TreeContext* tree);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_TREE_INTERNAL_H_
