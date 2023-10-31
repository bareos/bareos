/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
// Written by Marco van Wieringen, December 2011
/**
 * @file
 * BAREOS Director -- Import/Export and Move functions.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "dird/ua_label.h"
#include "dird/ua_impexp.h"
#include "dird/ua_update.h"
#include "dird/ua_select.h"
#include "lib/edit.h"

namespace directordaemon {

/* Forward referenced functions */

/**
 * Import/Export and Move volumes in an autochanger.
 *
 * The following things apply here:
 * - the source and destination slot list is walked in order
 *   So when you give a selection of 1-3,7,5 it will visit the
 *   slots 1,2,3,5,7 in that order.
 * - moving volumes from a source slot specification to a
 *   destination slot specification also is performed in order.
 *   So when you specify the source slots as 1-3,7,5 and
 *   the destination slots as 22-24,25,27 the following moves
 *   will take place:
 *      1 --> 22
 *      2 --> 23
 *      3 --> 24
 *      5 --> 25
 *      7 --> 27
 *
 * When you want to be sure the moves are performed in the
 * way you expect them to happen make sure the selection
 * cannot be wrongly interpreted by the code e.g. use
 * unambigious ranges. Or ranges of only one slot for
 * both the source and destination.
 */

/**
 * Walk the slot list and count the number of slots enabled in
 * the list.
 */
static inline slot_number_t count_enabled_slots(char* slot_list,
                                                slot_number_t max_slots)
{
  slot_number_t i;
  slot_number_t cnt = 0;

  for (i = 0; i < max_slots; i++) {
    if (BitIsSet(i, slot_list)) { cnt++; }
  }
  return cnt;
}

/**
 * See if a selected slot list has the wanted status and
 * deselect any slot which has not.
 */
static inline void validate_slot_list(UaContext* ua,
                                      StorageResource* store,
                                      changer_vol_list_t* vol_list,
                                      char* slot_list,
                                      slot_status_t status)
{
  vol_list_t* vl;

  // Walk the list of drives and slots available.
  foreach_dlist (vl, vol_list->contents) {
    switch (vl->slot_type) {
      case slot_type_t::kSlotTypeStorage:
      case slot_type_t::kSlotTypeImport:
        if (BitIsSet(vl->bareos_slot_number - 1, slot_list)) {
          switch (status) {
            case slot_status_t::kSlotStatusFull:
              // If it has the correct status we are done.
              if (vl->slot_status == status) { continue; }
              /* If the request is for a slot with status
               * we check the actual status. When its empty
               * but loaded in the drive we just pretend
               * that it has status. We just unload the drive
               * on the export move operation. */
              if (vl->slot_type == slot_type_t::kSlotTypeStorage
                  && vl->slot_status == slot_status_t::kSlotStatusEmpty
                  && vol_is_loaded_in_drive(store, vol_list,
                                            vl->bareos_slot_number)
                         != NULL) {
                continue;
              }
              Dmsg1(100, "Deselecting slot %hd doesn't have wanted status.\n",
                    vl->bareos_slot_number);
              ua->WarningMsg(
                  T_("Deselecting slot %hd doesn't have wanted status.\n"),
                  vl->bareos_slot_number);
              ClearBit(vl->bareos_slot_number - 1, slot_list);
              break;
            case slot_status_t::kSlotStatusEmpty:
              // If it has the correct status we are done.
              if (vl->slot_status == status) { continue; }
              /* If the slot is empty and this is normal slot
               * make sure its not loaded in a drive because
               * then the slot is not really empty. */
              if (vl->slot_type == slot_type_t::kSlotTypeStorage
                  && vl->slot_status == slot_status_t::kSlotStatusEmpty
                  && vol_is_loaded_in_drive(store, vol_list,
                                            vl->bareos_slot_number)
                         == NULL) {
                continue;
              }
              Dmsg1(100, "Deselecting slot %hd doesn't have wanted status.\n",
                    vl->bareos_slot_number);
              ua->WarningMsg(
                  T_("Deselecting slot %hd doesn't have wanted status.\n"),
                  vl->bareos_slot_number);
              ClearBit(vl->bareos_slot_number - 1, slot_list);
              break;
            default:
              break;
          }
        }
        break;
      default:
        break;
    }
  }
}

/**
 * See where a certain slot is referenced.
 * For a drive slot we check the loaded variable
 * and for all other slots the exact slotnr.
 * We only check slots which have content.
 */
static inline vol_list_t* find_slot_in_list(changer_vol_list_t* vol_list,
                                            slot_number_t slotnr)
{
  vol_list_t* vl;

  foreach_dlist (vl, vol_list->contents) {
    switch (vl->slot_status) {
      case slot_status_t::kSlotStatusFull:
        switch (vl->slot_type) {
          case slot_type_t::kSlotTypeDrive:
            if (vl->currently_loaded_slot_number == slotnr) { return vl; }
            break;
          default:
            if (vl->bareos_slot_number == slotnr) { return vl; }
            break;
        }
        break;
      default:
        continue;
    }
  }
  return NULL;
}

/**
 * Check if a source and destination slot list overlap.
 * An overlap is solved when there is a slot enabled
 * in either the source or destination slot list before
 * the overlap is detected. e.g. then on a move the volume
 * is first moved somewhere else before either the source
 * or destination slot is referenced by the next operation.
 * Then again it wise not to perform to crazy operations
 * as we will cancel any crazy-ness as soon as we encounter
 * it.
 */
static inline bool slot_lists_overlap(char* src_slot_list,
                                      char* dst_slot_list,
                                      slot_number_t max_slots)
{
  slot_number_t i;
  bool other_slot_enabled = false;

  for (i = 0; i < max_slots; i++) {
    /* See if both the source and destination slot are selected
     * and there has not been a source or destination slot
     * which has been selected before. */
    if (BitIsSet(i, src_slot_list) && BitIsSet(i, dst_slot_list)
        && !other_slot_enabled) {
      Dmsg0(100,
            "Found slot enabled in either source or destination selection "
            "which overlap\n");
      return true;
    } else {
      if (BitIsSet(i, src_slot_list) || BitIsSet(i, dst_slot_list)) {
        Dmsg0(100,
              "Found slot enabled in either source or destination selection\n");
        other_slot_enabled = true;
      }
    }
  }
  Dmsg0(100,
        "Found no slot enabled in either source or destination selection which "
        "overlap\n");
  return false;
}

/**
 * Scan all slots that are not empty for the exact volumename
 * by reading the label of the volume replacing the scanned
 * barcode when available. When a valid source slot list
 * is given we only check the slots enabled in that slot list.
 * We return an updated changer_vol_list_t with the new content
 * of the autochanger after the scan as that may move some
 * volumes around. We free the old list and return the new.
 */
static inline changer_vol_list_t* scan_slots_for_volnames(
    UaContext* ua,
    StorageResource* store,
    drive_number_t drive,
    changer_vol_list_t* vol_list,
    char* src_slot_list)
{
  changer_vol_list_t* new_vol_list;
  vol_list_t vls;
  vol_list_t *vl1, *vl2;

  // Walk the list of drives and slots available.
  foreach_dlist (vl1, vol_list->contents) {
    switch (vl1->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        continue;
      default:
        /* See if a slot list selection was done and
         * if so only get the content for this slot when
         * it is selected in the slot list. */
        if (src_slot_list
            && !BitIsSet(vl1->bareos_slot_number - 1, src_slot_list)) {
          continue;
        }

        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusFull:
            if (vl1->VolName) {
              free(vl1->VolName);
              vl1->VolName = NULL;
            }
            vl1->VolName
                = get_volume_name_from_SD(ua, vl1->bareos_slot_number, drive);
            Dmsg2(100, "Got Vol=%s from SD for Slot=%hd\n", vl1->VolName,
                  vl1->bareos_slot_number);
            break;
          case slot_status_t::kSlotStatusEmpty:
            /* See if the slot is empty because the volume is
             * loaded in a drive. */
            if (vl1->slot_type == slot_type_t::kSlotTypeStorage
                && (vl2 = vol_is_loaded_in_drive(store, vol_list,
                                                 vl1->bareos_slot_number))
                       != NULL) {
              if (vl2->VolName) {
                free(vl2->VolName);
                vl2->VolName = NULL;
              }
              vl2->VolName
                  = get_volume_name_from_SD(ua, vl1->bareos_slot_number, drive);
              Dmsg2(100, "Got Vol=%s from SD for Slot=%hd\n", vl2->VolName,
                    vl1->bareos_slot_number);
            }
            break;
          default:
            continue;
        }
        break;
    }
  }

  /* As the scan for volumes can alter the location of
   * the volumes in the autochanger e.g. volumes in drives
   * being put back into slots etc we rescan the changer. */
  new_vol_list = get_vol_list_from_storage(ua, store, true /* listall */,
                                           true /* want to see all slots */,
                                           false /* non cached list */);
  if (!new_vol_list) {
    // Free the old vol_list and return a NULL vol_list.
    StorageFreeVolList(store, vol_list);
    return NULL;
  }

  /* Walk the list of drives and slots available.
   * And copy the new scanned volume names from the old list
   * to the new list.
   *
   * This is optimized for the case the slots are still
   * filled with the same volume. */
  foreach_dlist (vl1, new_vol_list->contents) {
    switch (vl1->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusFull:
            // Lookup the drive in the old list.
            vls.element_address = vl1->element_address;
            vl2 = (vol_list_t*)vol_list->contents->binary_search(
                &vls, StorageCompareVolListEntry);
            if (vl2 && vl2->slot_status == slot_status_t::kSlotStatusFull
                && vl2->currently_loaded_slot_number
                       == vl1->currently_loaded_slot_number) {
              // Volume in drive is the same copy the volume name.
              if (vl2->VolName) { free(vl2->VolName); }
              vl2->VolName = vl1->VolName;
              vl1->VolName = NULL;
            } else {
              /* Drive is loaded with a volume which was previously
               * loaded somewhere else. Lookup the currently loaded
               * volume in the old list. */
              vl2 = find_slot_in_list(vol_list,
                                      vl1->currently_loaded_slot_number);
              if (vl2) {
                if (vl2->VolName) { free(vl2->VolName); }
                vl2->VolName = vl1->VolName;
                vl1->VolName = NULL;
              }
            }
            break;
          default:
            continue;
        }
        break;
      case slot_type_t::kSlotTypeStorage:
      case slot_type_t::kSlotTypeImport:
        /* See if a slot list selection was done and
         * if so only get the content for this slot when
         * it is selected in the slot list. */
        if (src_slot_list
            && !BitIsSet(vl1->bareos_slot_number - 1, src_slot_list)) {
          continue;
        }
        switch (vl1->slot_status) {
          case slot_status_t::kSlotStatusFull:
            // Lookup the slot in the old list.
            vls.element_address = vl1->element_address;
            vl2 = (vol_list_t*)vol_list->contents->binary_search(
                &vls, StorageCompareVolListEntry);
            if (vl2 && vl2->slot_status == slot_status_t::kSlotStatusFull
                && vl2->bareos_slot_number == vl1->bareos_slot_number) {
              // Volume in slot is the same copy the volume name.
              if (vl2->VolName) { free(vl2->VolName); }
              vl2->VolName = vl1->VolName;
              vl1->VolName = NULL;
            } else {
              /* This should never happen as a volume is always put back
               * into the same slot it was taken from. But as we have the
               * code to lookup the old place we take a shot at it. */
              vl2 = find_slot_in_list(vol_list, vl1->bareos_slot_number);
              if (vl2) {
                if (vl2->VolName) { free(vl2->VolName); }
                vl2->VolName = vl1->VolName;
                vl1->VolName = NULL;
              }
            }
            break;
          default:
            continue;
        }
        break;
      default:
        break;
    }
  }

  // Free the old vol_list and return the new data.
  StorageFreeVolList(store, vol_list);
  return new_vol_list;
}

// Convert a volume name into a slot selection.
static inline bool get_slot_list_using_volname(UaContext* ua,
                                               StorageResource* store,
                                               const char* volumename,
                                               changer_vol_list_t* vol_list,
                                               char* wanted_slot_list,
                                               char* selected_slot_list)
{
  vol_list_t *vl1, *vl2;
  bool found = false;

  if (IsNameValid(volumename)) {
    foreach_dlist (vl1, vol_list->contents) {
      // We only select normal and import/export slots.
      switch (vl1->slot_type) {
        case slot_type_t::kSlotTypeStorage:
        case slot_type_t::kSlotTypeImport:
          /* When the source slot list is limited we check to
           * see if this slot should be taken into consideration. */
          if (wanted_slot_list
              && !BitIsSet(vl1->bareos_slot_number - 1, wanted_slot_list)) {
            continue;
          }

          switch (vl1->slot_status) {
            case slot_status_t::kSlotStatusFull:
              // See if the wanted volume is loaded in this slot.
              Dmsg3(
                  100,
                  "Checking for volume name in slot %hd, wanted %s, found %s\n",
                  vl1->bareos_slot_number, volumename,
                  (vl1->VolName) ? vl1->VolName : "NULL");
              if (vl1->VolName && bstrcmp(vl1->VolName, volumename)) {
                found = true;
              }
              break;
            case slot_status_t::kSlotStatusEmpty:
              /* See if this slot is loaded in drive and drive contains wanted
               * volume */
              vl2 = vol_is_loaded_in_drive(store, vol_list,
                                           vl1->bareos_slot_number);
              if (vl2 != NULL) {
                Dmsg3(100,
                      "Checking for volume name in drive %hd, wanted %s, found "
                      "%s\n",
                      vl2->bareos_slot_number, volumename,
                      (vl2->VolName) ? vl2->VolName : "NULL");
                if (vl2->VolName && bstrcmp(vl2->VolName, volumename)) {
                  found = true;
                }
              } else {
                Dmsg1(100, "Skipping empty slot %hd\n",
                      vl1->bareos_slot_number);
              }
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }

      // If we found a match break the loop.
      if (found) { break; }
    }

    /* See if we found the wanted volumename in the list
     * of available slots in the autochanger and mark the
     * slot in the slot_list or give a warning when the
     * volumename was not found. */
    if (found) {
      SetBit(vl1->bareos_slot_number - 1, selected_slot_list);
    } else {
      Dmsg1(100, "No volume named %s in changer or in selected source slots.\n",
            volumename);
      ua->WarningMsg(
          T_("No volume named %s in changer or in selected source slots.\n"),
          volumename);
    }
  } else {
    Dmsg1(100, "Skipping illegal volumename %s.\n", volumename);
    ua->WarningMsg(T_("Skipping illegal volumename %s.\n"), volumename);
  }

  return found;
}

// Convert a number of volume names into a slot selection.
static inline slot_number_t get_slot_list_using_volnames(
    UaContext* ua,
    StorageResource* store,
    int arg,
    changer_vol_list_t* vol_list,
    char* wanted_slot_list,
    char* selected_slot_list)
{
  slot_number_t i;
  slot_number_t cnt = 0;
  char *s, *token, *sep;

  /* The arg argument contains the index of the first occurence
   * of the volume keyword. We scan the whole cmdline for one
   * or more volume= cmdline parameters. */
  for (i = arg; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], "volume")) {
      // Parse a volumelist e.g. vol1|vol2 and a single volume e.g. vol1
      s = strdup(ua->argv[i]);
      token = s;

      /* We could use strtok() here. But we're not going to, because:
       * (a) strtok() is deprecated, having been replaced by strsep();
       * (b) strtok() is broken in significant ways.
       * we could use strsep() instead, but it's not universally available.
       * so we grow our own using strchr(). */
      sep = strchr(token, '|');
      while (sep != NULL) {
        *sep = '\0';
        if (get_slot_list_using_volname(ua, store, token, vol_list,
                                        wanted_slot_list, selected_slot_list)) {
          cnt++;
        }
        token = ++sep;
        sep = strchr(token, '|');
      }

      // Pick up the last token.
      if (*token) {
        if (get_slot_list_using_volname(ua, store, token, vol_list,
                                        wanted_slot_list, selected_slot_list)) {
          cnt++;
        }
      }

      free(s);
    }
  }
  return cnt;
}

/**
 * Create a slot list selection based on the slot type
 * and slot content. All slots which have the wanted
 * slot type and wanted slot content are selected.
 */
static inline slot_number_t auto_fill_slot_selection(
    StorageResource* store,
    changer_vol_list_t* vol_list,
    char* slot_list,
    slot_type_t type,
    slot_status_t content)
{
  slot_number_t cnt = 0;
  vol_list_t* vl;

  // Walk the list of drives and slots available.
  foreach_dlist (vl, vol_list->contents) {
    // Make sure slot_type and slot_status match.
    if (vl->slot_type != type || vl->slot_status != content) {
      Dmsg3(100, "Skipping slot %hd, Type %hd, Content %hd\n",
            vl->bareos_slot_number, vl->slot_type, vl->slot_status);
      continue;
    }

    /* If the slot is empty and this is normal slot
     * make sure its not loaded in a drive because
     * then the slot is not really empty. */
    if (type == slot_type_t::kSlotTypeStorage
        && content == slot_status_t::kSlotStatusEmpty
        && vol_is_loaded_in_drive(store, vol_list, vl->bareos_slot_number)
               != NULL) {
      Dmsg3(100,
            "Skipping slot %hd, Type %hd, Content %hd is empty but loaded in "
            "drive\n",
            vl->bareos_slot_number, vl->slot_type, vl->slot_status);
      continue;
    }

    /* Mark the slot as selected in the slot list.
     * And increase the number of slots selected. */
    Dmsg3(100,
          "Selected slot %hd which has slot_type %hd and content_type %hd\n",
          vl->bareos_slot_number, vl->slot_type, vl->slot_status);
    SetBit(vl->bareos_slot_number - 1, slot_list);
    cnt++;
  }
  return cnt;
}

/**
 * Verify if all slots in the given slot list are of a certain
 * type and have a given content.
 */
static inline bool verify_slot_list(StorageResource* store,
                                    changer_vol_list_t* vol_list,
                                    char* slot_list,
                                    slot_type_t type,
                                    slot_status_t content)
{
  vol_list_t* vl;

  // Walk the list of drives and slots available.
  foreach_dlist (vl, vol_list->contents) {
    /* Move operations are only allowed between
     * normal slots and import/export slots so
     * don't consider any other slot type. */
    switch (vl->slot_type) {
      case slot_type_t::kSlotTypeStorage:
      case slot_type_t::kSlotTypeImport:
        if (BitIsSet(vl->bareos_slot_number - 1, slot_list)) {
          // If the type and content is ok we can continue with the next one.
          if (vl->slot_type == type && vl->slot_status == content) { continue; }

          /* When the content is not the wanted and this is a normal
           * slot take into consideration if its loaded into the drive.
           * When we are asked for an empty slot it should NOT be loaded
           * in the drive but when we are asked for a full slot it being
           * loaded in the drive also makes that the slot is filled as
           * we can just release the drive so that its put back into
           * the slot and then moved. */
          if (vl->slot_type == slot_type_t::kSlotTypeStorage) {
            switch (content) {
              case slot_status_t::kSlotStatusEmpty:
                if (vol_is_loaded_in_drive(store, vol_list,
                                           vl->bareos_slot_number)
                    != NULL) {
                  Dmsg3(100,
                        "Skipping slot %hd, Type %hd, Content %hd is empty but "
                        "loaded in drive\n",
                        vl->bareos_slot_number, vl->slot_type, vl->slot_status);
                  return false;
                }
                break;
              case slot_status_t::kSlotStatusFull:
                if (vol_is_loaded_in_drive(store, vol_list,
                                           vl->bareos_slot_number)
                    != NULL) {
                  continue;
                }
                break;
              default:
                break;
            }
          }

          // Not the wanted type or content and not a special case.
          Dmsg3(100, "Skipping slot %hd, Type %hd, Content %hd\n",
                vl->bareos_slot_number, vl->slot_type, vl->slot_status);
          return false;
        }
        break;
      default:
        break;
    }
  }
  return true;
}

/**
 * Perform an internal update of our view of the autochanger on a move
 * instruction without requesting the new status from the SD again.
 */
static inline bool update_internal_slot_list(changer_vol_list_t* vol_list,
                                             slot_number_t source,
                                             slot_number_t destination)
{
  bool found;
  vol_list_t *vl1, *vl2;

  // First lookup the source and destination slots in the vol_list.
  found = false;
  foreach_dlist (vl1, vol_list->contents) {
    switch (vl1->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        continue;
      default:
        if (vl1->bareos_slot_number == source) { found = true; }
        break;
    }
    if (found) { break; }
  }

  found = false;
  foreach_dlist (vl2, vol_list->contents) {
    switch (vl2->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        continue;
      default:
        if (vl2->bareos_slot_number == destination) { found = true; }
        break;
    }
    if (found) { break; }
  }

  if (vl1 && vl2) {
    // Swap the data.
    vl2->VolName = vl1->VolName;
    vl2->slot_status = slot_status_t::kSlotStatusFull;
    vl1->VolName = NULL;
    vl1->slot_status = slot_status_t::kSlotStatusEmpty;
    Dmsg5(100,
          "Update internal slotlist slot %hd with volname %s, content %hd and "
          "slot %hd with content %hd and volname NULL\n",
          vl2->bareos_slot_number, (vl2->VolName) ? vl2->VolName : "NULL",
          vl2->slot_status, vl1->bareos_slot_number, vl1->slot_status);
    return true;
  }
  return false;
}

// Unload a volume currently loaded in a drive.
static bool release_loaded_volume(UaContext* ua,
                                  StorageResource* store,
                                  drive_number_t drive,
                                  changer_vol_list_t* vol_list)
{
  bool found;
  vol_list_t *vl1, *vl2;

  if (!DoAutochangerVolumeOperation(ua, store, "release", drive, -1)) {
    return false;
  }

  // Lookup the drive in the vol_list.
  found = false;
  foreach_dlist (vl1, vol_list->contents) {
    switch (vl1->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        if (vl1->bareos_slot_number == drive) { found = true; }
        break;
      default:
        break;
    }
    /* As drives are at the front of the list
     * when we see the first non drive we are done. */
    if (found || vl1->slot_type != slot_type_t::kSlotTypeDrive) { break; }
  }

  /* Lookup the slot in the slotlist referenced by the loaded value in the drive
   * slot. */
  found = false;
  foreach_dlist (vl2, vol_list->contents) {
    switch (vl2->slot_type) {
      case slot_type_t::kSlotTypeDrive:
        continue;
      default:
        if (vl2->bareos_slot_number == vl1->currently_loaded_slot_number) {
          found = true;
        }
        break;
    }
    if (found) { break; }
  }

  if (vl1 && vl2) {
    // Swap the data.
    vl2->VolName = vl1->VolName;
    vl2->slot_status = slot_status_t::kSlotStatusFull;
    vl1->VolName = NULL;
    vl1->slot_status = slot_status_t::kSlotStatusEmpty;
    vl1->currently_loaded_slot_number = 0;
    Dmsg5(100,
          "Update internal slotlist slot %hd with volname %s, content %hd and "
          "slot %hd with content %hd and volname NULL\n",
          vl2->bareos_slot_number, (vl2->VolName) ? vl2->VolName : "NULL",
          vl2->slot_status, vl1->bareos_slot_number, vl1->slot_status);
    return true;
  }
  return false;
}

/**
 * Ask the autochanger to move volume from a source slot
 * to a destination slot by walking the two filled
 * slot lists and marking every visited slot.
 */
static char* move_volumes_in_autochanger(UaContext* ua,
                                         enum e_move_op operation,
                                         StorageResource* store,
                                         changer_vol_list_t* vol_list,
                                         char* src_slot_list,
                                         char* dst_slot_list,
                                         slot_number_t max_slots)
{
  vol_list_t* vl;
  slot_number_t transfer_from, transfer_to;
  char* visited_slot_list;
  slot_number_t nr_enabled_src_slots, nr_enabled_dst_slots;

  // Sanity check.
  nr_enabled_src_slots = count_enabled_slots(src_slot_list, max_slots);
  nr_enabled_dst_slots = count_enabled_slots(dst_slot_list, max_slots);
  if (nr_enabled_src_slots == 0 || nr_enabled_dst_slots == 0) {
    ua->WarningMsg(T_("Nothing to do\n"));
    return NULL;
  }

  /* When doing an export we first set all slots in the database
   * to inchanger = 0 so we cannot get surprises that a running
   * backup can grab such a volume. */
  switch (operation) {
    case VOLUME_EXPORT:
      UpdateInchangerForExport(ua, store, vol_list, src_slot_list);
      break;
    default:
      break;
  }

  /* Create an empty slot list in which we keep track of the slots
   * we visited during this move operation so we can return that data
   * to the caller which can use it to update only the slots updated
   * by the move operation. */
  visited_slot_list = (char*)malloc(NbytesForBits(max_slots));
  ClearAllBits(max_slots, visited_slot_list);

  transfer_to = 1;
  for (transfer_from = 1; transfer_from <= max_slots; transfer_from++) {
    // See if the slot is marked in the source slot list.
    if (BitIsSet(transfer_from - 1, src_slot_list)) {
      // Search for the first marked slot in the destination selection.
      while (transfer_to <= max_slots) {
        if (BitIsSet(transfer_to - 1, dst_slot_list)) { break; }
        transfer_to++;
      }

      // This should never happen but a sanity check just in case.
      if (transfer_to > max_slots) {
        Dmsg0(100, "Failed to find suitable destination slot in slot range.\n");
        ua->WarningMsg(
            T_("Failed to find suitable destination slot in slot range.\n"));
        break;
      }

      // Based on the operation see if we need to unload the drive.
      switch (operation) {
        case VOLUME_EXPORT:
          /* Sanity check to see if the volume being exported is in the drive.
           * If so we release the drive and then perform the actual move. */
          vl = vol_is_loaded_in_drive(store, vol_list, transfer_from);
          if (vl != NULL) {
            if (!release_loaded_volume(ua, store, vl->bareos_slot_number,
                                       vol_list)) {
              Dmsg1(100, "Failed to release volume in drive %hd\n",
                    vl->bareos_slot_number);
              ua->WarningMsg(T_("Failed to release volume in drive %hd\n"),
                             vl->bareos_slot_number);
              continue;
            }
          }
          break;
        default:
          break;
      }

      // If we found a source and destination slot perform the move.
      if (transfer_volume(ua, store, transfer_from, transfer_to)) {
        Dmsg2(100,
              "Successfully moved volume from source slot %hd to destination "
              "slot %hd\n",
              transfer_from, transfer_to);
        update_internal_slot_list(vol_list, transfer_from, transfer_to);
        SetBit(transfer_from - 1, visited_slot_list);
        SetBit(transfer_to - 1, visited_slot_list);
      } else {
        Dmsg2(100,
              "Failed to move volume from source slot %hd to destination slot "
              "%hd\n",
              transfer_from, transfer_to);
        ua->WarningMsg(T_("Failed to move volume from source slot %hd to "
                          "destination slot %hd\n"),
                       transfer_from, transfer_to);
        switch (operation) {
          case VOLUME_EXPORT:
            /* For an export we always set the source slot as visited so when
             * the move operation fails we update the inchanger flag in the
             * database back to 1 so we know it still is in the changer. */
            SetBit(transfer_from - 1, visited_slot_list);
            break;
          default:
            break;
        }
      }
      transfer_to++;
    }
  }

  return visited_slot_list;
}

/**
 * Perform the actual move operation which is either a:
 * - import of import slots into normal slots
 * - export of normal slots into export slots
 * - move from one normal slot to another normal slot
 */
static bool PerformMoveOperation(UaContext* ua, enum e_move_op operation)
{
  bool scan;
  UnifiedStorageResource store;
  changer_vol_list_t* vol_list;
  char *src_slot_list = NULL, *dst_slot_list = NULL, *tmp_slot_list = NULL,
       *visited_slot_list = NULL;
  slot_number_t nr_enabled_src_slots = 0, nr_enabled_dst_slots = 0;
  drive_number_t drive = kInvalidDriveNumber;
  slot_number_t max_slots;
  int list_index;
  bool retval = false;

  store.store = get_storage_resource(ua, false, true);
  if (!store.store) { return retval; }

  PmStrcpy(store.store_source, T_("command line"));
  SetWstorage(ua->jcr, &store);

  /* See if the scan option was given.
   * We need a drive for the scanning so ask if
   * the scan option was specified. */
  scan = FindArg(ua, NT_("scan")) >= 0;
  if (scan) { drive = GetStorageDrive(ua, store.store); }

  /* Get the number of slots in the autochanger for
   * sizing the slot lists. */
  max_slots = GetNumSlots(ua, store.store);
  if (max_slots <= 0) {
    ua->WarningMsg(T_("No slots in changer.\n"));
    return retval;
  }

  /* Get the current content of the autochanger for
   * validation and selection purposes. */
  vol_list = get_vol_list_from_storage(ua, store.store, true /* listall */,
                                       true /* want to see all slots */);
  if (!vol_list) {
    ua->WarningMsg(T_("No Volumes found, or no barcodes.\n"));
    goto bail_out;
  }

  // See if there are any source slot selections.
  list_index = FindArgWithValue(ua, "srcslots");
  if (list_index < 0) { list_index = FindArgWithValue(ua, "srcslot"); }
  if (list_index >= 0) {
    src_slot_list = (char*)malloc(NbytesForBits(max_slots));
    ClearAllBits(max_slots, src_slot_list);
    if (!GetUserSlotList(ua, src_slot_list, "srcslots", max_slots)) {
      goto bail_out;
    } else {
      /* See if we should scan slots for the correct
       * volume name or that we can use the barcodes.
       * If a set of src slots was given we only scan
       * the content of those slots. */
      if (scan) {
        vol_list = scan_slots_for_volnames(ua, store.store, drive, vol_list,
                                           src_slot_list);
        if (!vol_list) { goto bail_out; }
      }

      // Clear any slot that has no content in the source selection.
      validate_slot_list(ua, store.store, vol_list, src_slot_list,
                         slot_status_t::kSlotStatusFull);
      nr_enabled_src_slots = count_enabled_slots(src_slot_list, max_slots);
    }
  } else {
    /* See if we should scan slots for the correct
     * volume name or that we can use the barcodes. */
    if (scan) {
      vol_list = scan_slots_for_volnames(ua, store.store, drive, vol_list,
                                         src_slot_list);
      if (!vol_list) { goto bail_out; }
    }
  }

  // See if there are any destination slot selections.
  list_index = FindArgWithValue(ua, "dstslots");
  if (list_index < 0) { list_index = FindArgWithValue(ua, "dstslot"); }
  if (list_index >= 0) {
    dst_slot_list = (char*)malloc(NbytesForBits(max_slots));
    ClearAllBits(max_slots, dst_slot_list);
    if (!GetUserSlotList(ua, dst_slot_list, "dstslots", max_slots)) {
      goto bail_out;
    } else {
      /* Clear any slot in the destination slot list which has not the wanted
       * content. */
      switch (operation) {
        case VOLUME_IMPORT:
        case VOLUME_EXPORT:
          validate_slot_list(ua, store.store, vol_list, dst_slot_list,
                             slot_status_t::kSlotStatusEmpty);
          break;
        default:
          break;
      }
      nr_enabled_dst_slots = count_enabled_slots(dst_slot_list, max_slots);
    }
  }

  /* For Import and Export operations we can also use a list
   * of volume names for which we lookup the slot they are
   * loaded in. */
  switch (operation) {
    case VOLUME_IMPORT:
    case VOLUME_EXPORT:
      list_index = FindArgWithValue(ua, "volume");
      if (list_index > 0) {
        /* Create a new temporary slot list with gets filled
         * by the selection criteria on the cmdline. We provide
         * the current src_slot_list as an extra selection criteria. */
        tmp_slot_list = (char*)malloc(NbytesForBits(max_slots));
        ClearAllBits(max_slots, tmp_slot_list);
        nr_enabled_src_slots = get_slot_list_using_volnames(
            ua, store.store, list_index, vol_list, src_slot_list,
            tmp_slot_list);
        if (src_slot_list) { free(src_slot_list); }
        src_slot_list = tmp_slot_list;
      }
      break;
    default:
      break;
  }

  Dmsg2(100, "src_slots = %hd, dst_slots = %hd\n", nr_enabled_src_slots,
        nr_enabled_dst_slots);

  /* First generic sanity check if there is a source selection the number
   * of selected slots in the source must be less or equal to the
   * number of slots in the destination */
  if (nr_enabled_src_slots && nr_enabled_dst_slots
      && nr_enabled_src_slots > nr_enabled_dst_slots) {
    ua->WarningMsg(
        T_("Source slot selection doesn't fit into destination slot "
           "selection.\n"));
    goto bail_out;
  }

  // Detect any conflicting overlaps in source and destination selection.
  if (nr_enabled_src_slots && nr_enabled_dst_slots
      && slot_lists_overlap(src_slot_list, dst_slot_list, max_slots)) {
    ua->WarningMsg(
        T_("Source slot selection and destination slot selection overlap.\n"));
    goto bail_out;
  }

  // Operation specific checks.
  switch (operation) {
    case VOLUME_EXPORT:
      if (nr_enabled_src_slots == 0) {
        ua->WarningMsg(
            T_("Cannot perform an export operation without source slot "
               "selection\n"));
        goto bail_out;
      }
      break;
    case VOLUME_MOVE:
      if (nr_enabled_src_slots == 0 || nr_enabled_dst_slots == 0) {
        ua->WarningMsg(
            T_("Cannot perform a move operation without source and/or "
               "destination selection\n"));
        goto bail_out;
      }
      break;
    default:
      break;
  }

  switch (operation) {
    case VOLUME_IMPORT:
      // Perform an autofill of the source slots when none are selected.
      if (nr_enabled_src_slots == 0) {
        src_slot_list = (char*)malloc(NbytesForBits(max_slots));
        ClearAllBits(max_slots, src_slot_list);
        nr_enabled_src_slots = auto_fill_slot_selection(
            store.store, vol_list, src_slot_list, slot_type_t::kSlotTypeImport,
            slot_status_t::kSlotStatusFull);
      } else {
        /* All slots in the source selection need to be import/export slots and
         * filled. */
        if (!verify_slot_list(store.store, vol_list, src_slot_list,
                              slot_type_t::kSlotTypeImport,
                              slot_status_t::kSlotStatusFull)) {
          ua->WarningMsg(
              T_("Not all slots in source selection are import slots and "
                 "filled.\n"));
          goto bail_out;
        }
      }
      // Perform an autofill of the destination slots when none are selected.
      if (nr_enabled_dst_slots == 0) {
        dst_slot_list = (char*)malloc(NbytesForBits(max_slots));
        ClearAllBits(max_slots, dst_slot_list);
        nr_enabled_dst_slots = auto_fill_slot_selection(
            store.store, vol_list, dst_slot_list, slot_type_t::kSlotTypeStorage,
            slot_status_t::kSlotStatusEmpty);
        if (nr_enabled_src_slots > nr_enabled_dst_slots) {
          ua->WarningMsg(
              T_("Not enough free slots available to import %hd volumes\n"),
              nr_enabled_src_slots);
          goto bail_out;
        }
      } else {
        /* All slots in the destination selection need to be normal slots and
         * empty. */
        if (!verify_slot_list(store.store, vol_list, dst_slot_list,
                              slot_type_t::kSlotTypeStorage,
                              slot_status_t::kSlotStatusEmpty)) {
          ua->WarningMsg(
              T_("Not all slots in destination selection are normal slots and "
                 "empty.\n"));
          goto bail_out;
        }
      }
      visited_slot_list = move_volumes_in_autochanger(
          ua, operation, store.store, vol_list, src_slot_list, dst_slot_list,
          max_slots);
      break;
    case VOLUME_EXPORT:
      // All slots in the source selection need to be normal slots.
      if (!verify_slot_list(store.store, vol_list, src_slot_list,
                            slot_type_t::kSlotTypeStorage,
                            slot_status_t::kSlotStatusFull)) {
        ua->WarningMsg(
            T_("Not all slots in source selection are normal slots and "
               "filled.\n"));
        goto bail_out;
      }
      // Perform an autofill of the destination slots when none are selected.
      if (nr_enabled_dst_slots == 0) {
        dst_slot_list = (char*)malloc(NbytesForBits(max_slots));
        ClearAllBits(max_slots, dst_slot_list);
        nr_enabled_dst_slots = auto_fill_slot_selection(
            store.store, vol_list, dst_slot_list, slot_type_t::kSlotTypeImport,
            slot_status_t::kSlotStatusEmpty);
        if (nr_enabled_src_slots > nr_enabled_dst_slots) {
          ua->WarningMsg(T_("Not enough free export slots available to export "
                            "%hd volume%s\n"),
                         nr_enabled_src_slots,
                         nr_enabled_src_slots > 1 ? "s" : "");
          goto bail_out;
        }
      } else {
        /* All slots in the destination selection need to be import/export slots
         * and empty. */
        if (!verify_slot_list(store.store, vol_list, dst_slot_list,
                              slot_type_t::kSlotTypeImport,
                              slot_status_t::kSlotStatusEmpty)) {
          ua->WarningMsg(
              T_("Not all slots in destination selection are export slots and "
                 "empty.\n"));
          goto bail_out;
        }
      }
      visited_slot_list = move_volumes_in_autochanger(
          ua, operation, store.store, vol_list, src_slot_list, dst_slot_list,
          max_slots);
      break;
    case VOLUME_MOVE:
      visited_slot_list = move_volumes_in_autochanger(
          ua, operation, store.store, vol_list, src_slot_list, dst_slot_list,
          max_slots);
      break;
    default:
      break;
  }

  /* If we actually moved some volumes update the database with the
   * new info for those slots. */
  if (visited_slot_list
      && count_enabled_slots(visited_slot_list, max_slots) > 0) {
    Dmsg0(100, "Updating database with new info for visited slots\n");
    UpdateSlotsFromVolList(ua, store.store, vol_list, visited_slot_list);
  }

  retval = true;

bail_out:
  CloseSdBsock(ua);

  if (vol_list) { StorageReleaseVolList(store.store, vol_list); }
  if (src_slot_list) { free(src_slot_list); }
  if (dst_slot_list) { free(dst_slot_list); }
  if (visited_slot_list) { free(visited_slot_list); }

  return retval;
}

// Import volumes from Import/Export Slots into normal Slots.
bool ImportCmd(UaContext* ua, const char*)
{
  return PerformMoveOperation(ua, VOLUME_IMPORT);
}

// Export volumes from normal slots to Import/Export Slots.
bool ExportCmd(UaContext* ua, const char*)
{
  return PerformMoveOperation(ua, VOLUME_EXPORT);
}

// Move volume from one slot to another.
bool move_cmd(UaContext* ua, const char*)
{
  return PerformMoveOperation(ua, VOLUME_MOVE);
}
} /* namespace directordaemon */
