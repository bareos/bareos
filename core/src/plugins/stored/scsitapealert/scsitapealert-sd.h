/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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
#ifndef BAREOS_PLUGINS_STORED_SCSITAPEALERT_SCSITAPEALERT_SD_H_
#define BAREOS_PLUGINS_STORED_SCSITAPEALERT_SCSITAPEALERT_SD_H_

#include <array>
#include <cstdint>
#include "lib/message_severity.h"

namespace scsitapealert {

struct flag {
  unsigned int no;
  std::int32_t type;
  const char* name;
  const char* message;
  const char* cause;

  // compatible to encoding from GetTapealertFlags()
  bool present_in(uint64_t flags) const { return flags & (UINT64_C(1) << no); }
};

/* The following information is taken from the Tape Alert Specification v3.0
 * provided by Hewlett-Packard to NCITS for standardization activities.
 * see https://www.t10.org/ftp/t10/document.02/02-142r0.pdf */

constexpr std::array flags{
    flag{1, M_WARNING, "Read Warning",
         "The tape drive is having problems reading data. No data has been "
         "lost, "
         "but there has been a reduction in the performance of the tape.",
         "The drive is having severe trouble reading"},
    flag{2, M_WARNING, "Write Warning",
         "The tape drive is having problems writing data. No data has been "
         "lost, "
         "but there has been a reduction in the capacity of the tape.",
         "The drive is having severe trouble writing"},
    flag{
        3, M_WARNING, "Hard Error",
        "The operation has stopped because an error has occurred while reading "
        "or writing data which the drive cannot correct.",
        "The drive had a hard read or write error"},
    flag{4, M_ERROR, "Media",
         "Your data is at risk:\n"
         "1. Copy any data you require from this tape.\n"
         "2. Do not use this tape again.\n"
         "3. Restart the operation with a different tape.",
         "Media can no longer be written/read, or performance is severely "
         "degraded"},
    flag{5, M_ERROR, "Read Failure",
         "The tape is damaged or the drive is faulty. Call the tape drive "
         "supplier helpline.",
         "The drive can no longer read data from the tape"},
    flag{6, M_ERROR, "Write Failure",
         "The tape is from a faulty batch or the tape drive is faulty:\n"
         "1. Use a good tape to test the drive.\n"
         "2. If the problem persists, call the tape drive supplier helpline.",
         "The drive can no longer write data to the tape"},
    flag{7, M_WARNING, "Media Life",
         "The tape cartridge has reached the end of its calculated useful "
         "life:\n"
         "1. Copy any data you need to another tape\n"
         "2. Discard the old tape.",
         "The media has exceeded its specified life"},
    flag{8, M_WARNING, "Not Data Grade",
         "The tape cartridge is not data-grade. Any data you back up to the "
         "tape "
         "is at risk. Replace the cartridge with a data-grade tape.",
         "The drive has not been able to read the MRS stripes"},
    flag{9, M_ERROR, "Write Protect",
         "You are trying to write to a write-protected cartridge. Remove the "
         "write-protection or use another tape.",
         "Write command is attempted to a write protected tape"},
    flag{
        10, M_INFO, "No Removal",
        "You cannot eject the cartridge because the tape drive is in use. Wait "
        "until the operation is complete before ejecting the cartridge.",
        "Manual or s/w unload attempted when prevent media removal on"},
    flag{11, M_INFO, "Cleaning Media",
         "The tape in the drive is a cleaning cartridge.",
         "Cleaning tape loaded into drive"},
    flag{
        12, M_INFO, "Unsupported Format",
        "You have tried to load a cartridge of a type which is not supported "
        "by "
        "this drive.",
        "Attempted loaded of unsupported tape format, e.g. DDS2 in DDS1 drive"},
    flag{13, M_ERROR, "Recoverable Snapped Tape",
         "The operation has failed because the tape in the drive has snapped:\n"
         "1. Discard the old tape.\n"
         "2. Restart the operation with a different tape.",
         "Tape snapped/cut in the drive where media can be ejected"},
    flag{14, M_ERROR, "Unrecoverable Snapped Tape",
         "The operation has failed because the tape in the drive has snapped:\n"
         "1. Do not attempt to extract the tape cartridge.\n"
         "2. Call the tape drive supplier helpline.",
         "Tape snapped/cut in the drive where media cannot be ejected"},
    flag{15, M_WARNING, "Memory Chip in Cartridge Failure",
         "The memory in the tape cartridge has failed, which reduces "
         "performance. "
         "Do not use the cartridge for further backup operations.",
         "Memory chip failed in cartridge"},
    flag{16, M_ERROR, "Forced Eject",
         "The operation has failed because the tape cartridge was manually "
         "ejected while the tape drive was actively writing or reading.",
         "Manual or forced eject while drive actively writing or reading"},
    flag{17, M_WARNING, "Read Only Format",
         "You have loaded a cartridge of a type that is read-only in this "
         "drive. "
         "The cartridge will appear as write-protected.",
         "Media loaded that is read-only format"},
    flag{18, M_WARNING, "Tape Directory Corrupted on Load",
         "The directory on the tape cartridge has been corrupted. File search "
         "performance will be degraded. The tape directory can be rebuilt by "
         "reading all the data on the cartridge.",
         "Tape drive powered down with tape loaded, or permanent error "
         "prevented "
         "the tape directory being updated"},
    flag{19, M_INFO, "Nearing Media Life",
         "The tape cartridge is nearing the end of its calculated life. It is "
         "recommended that you:\n"
         "1. Use another tape cartridge for your next backup.\n"
         "2. Store this tape cartridge in a safe place in case you need to "
         "restore data from it.",
         "Media may have exceeded its specified number of passes"},
    flag{20, M_ERROR, "Clean Now",
         "The tape drive needs cleaning:\n"
         "1. If the operation has stopped, eject the tape and clean the drive\n"
         "2. If the operation has not stopped, wait for it to finish and then "
         "clean the drive.\n"
         "Check the tape drive users manual for device specific cleaning "
         "instructions.",
         "The drive thinks it has a head clog, or needs cleaning"},
    flag{21, M_WARNING, "Clean Periodic",
         "The tape drive is due for routine cleaning:\n"
         "1. Wait for the current operation to finish.\n"
         "2. Then use a cleaning cartridge.\n"
         "Check the tape drive users manual for device specific cleaning "
         "instructions.",
         "The drive is ready for a periodic clean"},
    flag{22, M_ERROR, "Expired Cleaning Media",
         "The last cleaning cartridge used in the tape drive has worn out:\n"
         "1. Discard the worn out cleaning cartridge.\n"
         "2. Wait for the current operation to finish.\n"
         "3. Then use a new cleaning cartridge.",
         "The cleaning tape has expired"},
    flag{23, M_ERROR, "Invalid Cleaning Tape",
         "The last cleaning cartridge used in the tape drive was an invalid "
         "type:\n"
         "1. Do not use this cleaning cartridge in this drive.\n"
         "2. Wait for the current operation to finish.\n"
         "3. Then use a valid cleaning cartridge.\n"
         "",
         "Invalid cleaning tape type used"},
    flag{24, M_WARNING, "Retention Requested",
         "The tape drive has requested a retention operation.",
         "The drive is having severe trouble reading or writing, which will be "
         "resolved by a retention cycle"},
    flag{25, M_WARNING, "Dual-Port Interface Error",
         "A redundant interface port on the tape drive has failed.",
         "Failure of one interface port in a dual-port configuration, e.g. "
         "Fibrechannel"},
    flag{26, M_WARNING, "Cooling Fan Failure",
         "A tape drive cooling fan has failed.",
         "Fan failure inside tape drive mechanism or tape drive enclosure"},
    flag{27, M_WARNING, "Power Supply",
         "A redundant power supply has failed inside the tape drive enclosure. "
         "Check the enclosure users manual for instructions on replacing the "
         "failed power supply.",
         "Redundant PSU failure inside the tape drive enclosure or rack "
         "subsystem"},
    flag{28, M_WARNING, "Power Consumption",
         "The tape drive power consumption is outside the specified range.",
         "Power consumption of the tape drive is outside specified range"},
    flag{29, M_WARNING, "Drive Maintenance",
         "Preventive maintenance of the tape drive is required. Check the tape "
         "drive users manual for device specific preventive maintenance tasks "
         "or "
         "call the tape drive supplier helpline.",
         "The drive requires preventative maintenance (not cleaning)."},
    flag{30, M_ERROR, "Hardware A",
         "The tape drive has a hardware fault:\n"
         "1. Eject the tape or magazine.\n"
         "2. Reset the drive.\n"
         "3. Restart the operation.",
         "The drive has a hardware fault that requires reset to recover."},
    flag{
        31, M_ERROR, "Hardware B",
        "The tape drive has a hardware fault:\n"
        "1. Turn the tape drive off and then on again.\n"
        "2. Restart the operation.\n"
        "3. If the problem persists, call the tape drive supplier helpline.\n"
        "Check the tape drive users manual for device specific instructions on "
        "turning the device power on and off.",
        "The drive has a hardware fault which is not read/write related or "
        "requires a power cycle to recover."},
    flag{32, M_WARNING, "Interface",
         "The tape drive has a problem with the host interface:\n"
         "1. Check the cables and cable connections.\n"
         "2. Restart the operation.",
         "The drive has identified an interfacing fault"},
    flag{33, M_ERROR, "Eject Media",
         "The operation has failed:\n"
         "1. Eject the tape or magazine.\n"
         "2. Insert the tape or magazine again.\n"
         "3. Restart the operation.",
         "Error recovery action"},
    flag{34, M_WARNING, "Download Fail",
         "The firmware download has failed because you have tried to use the "
         "incorrect firmware for this tape drive. Obtain the correct firmware "
         "and "
         "try again.",
         "Firmware download failed"},
    flag{35, M_WARNING, "Drive Humidity",
         "Environmental conditions inside the tape drive are outside the "
         "specified humidity range",
         "Drive humidity limits exceeded"},
    flag{36, M_WARNING, "Drive Temperature",
         "Environmental conditions inside the tape drive are outside the "
         "specified temperature range",
         "Drive temperature limits exceeded"},
    flag{37, M_WARNING, "Drive Voltage",
         "The voltage supply to the tape drive is outside the specified range",
         "Drive voltage limits exceeded"},
    flag{38, M_ERROR, "Predictive Failure",
         "A hardware failure of the tape drive is predicted. Call the tape "
         "drive "
         "supplier helpline.",
         "Predictive failure of drive hardware"},
    flag{
        39, M_WARNING, "Diagnostics Required",
        "The tape drive may have a fault. Check for availability of diagnostic "
        "information and run extended diagnostics if applicable. Check the "
        "tape "
        "drive users manual for instructions on running extended diagnostic "
        "tests and retrieving diagnostic data.",
        "The drive may have had a failure which may be identified by stored "
        "diagnostic information or by running extended diagnostics (eg Send "
        "Diagnostic)"},
    flag{
        40, M_ERROR, "Loader Hardware A",
        "The changer mechanism is having difficulty communicating with the "
        "tape "
        "drive:\n"
        "1. Turn the autoloader off then on.\n"
        "2. Restart the operation.\n"
        "3. If problem persists, call the tape drive supplier helpline.",
        "Loader mechanism is having trouble communicating with the tape drive"},
    flag{
        41, M_ERROR, "Loader Stray Tape",
        "A tape has been left in the autoloader by a previous hardware fault:\n"
        "1. Insert an empty magazine to clear the fault.\n"
        "2. If the fault does not clear, turn the autoloader off and then on "
        "again.\n"
        "3. If the problem persists, call the tape drive supplier helpline.",
        "Stray tape left in loader after previous error recovery"},
    flag{42, M_WARNING, "Loader Hardware B",
         "There is a problem with the autoloader mechanism.",
         "Loader mechanism has a hardware fault"},
    flag{43, M_ERROR, "Loader Door",
         "The operation has failed because the autoloader door is open:\n"
         "1. Clear any obstructions from the autoloader door.\n"
         "2. Eject the magazine and then insert it again.\n"
         "3. If the fault does not clear, turn the autoloader off and then on "
         "again\n"
         "4. If the problem persists, call the tape drive supplier helpline.",
         "Tape changer door open"},
    flag{
        44, M_ERROR, "Loader Hardware C",
        "The autoloader has a hardware fault:\n"
        "1. Turn the autoloader off and then on again.\n"
        "2. Restart the operation.\n"
        "3. If the problem persists, call the tape drive supplier helpline.\n"
        "Check the autoloader users manual for device specific instructions on "
        "turning the device power on and off.",
        "The loader mechanism has a hardware fault that is not mechanically "
        "related."},
    flag{45, M_ERROR, "Loader Magazine",
         "The autoloader cannot operate without the magazine.\n"
         "1. Insert the magazine into the autoloader\n"
         "2. Restart the operation.",
         "Loader magazine not present"},
    flag{46, M_WARNING, "Loader Predictive Failure",
         "A hardware failure of the changer mechanism is predicted. Call the "
         "tape "
         "drive supplier helpline.",
         "Predictive failure of loader mechanism hardware"},
    flag{50, M_WARNING, "Lost Statistics",
         "Media statistics have been lost at some time in the past.",
         "Drive or library powered down with tape loaded"},
    flag{51, M_WARNING, "Tape directory invalid at unload",
         "The tape directory on the tape cartridge just unloaded has been "
         "corrupted. File search performance will be degraded The tape "
         "directory "
         "can be rebuilt by reading all the data.",
         "Error prevented the tape directory being updated on unload."},
    flag{
        52, M_ERROR, "Tape system area write failure",
        "The tape just unloaded could not write its system area successfully:\n"
        "1. Copy data to another tape cartridge\n"
        "2. Discard the old cartridge",
        "Write errors while writing the system log on unload"},
    flag{53, M_ERROR, "Tape system area read failure",
         "The tape system area could not be read successfully at load time:\n"
         "1. Copy data to another tape cartridge\n"
         "2. Discard the old cartridge",
         "Read errors while reading the system area on load"},
    flag{54, M_ERROR, "No start of data",
         "The start of data could not be found on the tape:\n"
         "1. Check you are using the correct format tape\n"
         "2. Discard the tape or return the tape to your supplier.",
         "Tape damaged, bulk erased, or incorrect format"}};

}  // namespace scsitapealert
#endif  // BAREOS_PLUGINS_STORED_SCSITAPEALERT_SCSITAPEALERT_SD_H_
