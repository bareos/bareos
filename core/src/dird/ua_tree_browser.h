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
#ifndef BAREOS_DIRD_UA_TREE_BROWSER_H_
#define BAREOS_DIRD_UA_TREE_BROWSER_H_

namespace directordaemon {

class UaContext;
struct TreeContext;

// Why the interactive tree browser stopped running.
enum class TreeBrowserExit
{
  kDone,            /**< User chose to leave file selection entirely
                     *  (same as the classic "done" command). */
  kQuit,            /**< User aborted the restore ("quit"/"abort"). */
  kSwitchToClassic, /**< User pressed 'c': switch (back) to the classic
                     *  line-mode "$ " prompt for the rest of the
                     *  session. */
};

/**
 * True when ua's client is known to support the raw single-keystroke,
 * full-screen redraw protocol (BNET_START_SELECT/BNET_SELECT_INPUT) that
 * the interactive tree browser needs -- i.e. the same condition already
 * used for "supports_cursor_selection" in ua_select.cc's DoPrompt(). API
 * and batch clients never qualify, regardless of terminal size.
 */
bool TreeBrowserSupported(UaContext* ua);

/**
 * Run the full-screen, Midnight-Commander-style interactive restore tree
 * browser until the user leaves it. tree->node (current directory) and all
 * mark state are read and updated in place, exactly like the classic
 * commands, so switching between the browser and the classic prompt never
 * loses any state.
 */
TreeBrowserExit RunTreeBrowser(UaContext* ua, TreeContext* tree);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_TREE_BROWSER_H_
