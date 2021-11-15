/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *      The heap is managed like this:
 *
 *                      +----------------+
 *      table ------>   |     entry      |  <----- heap_top
 *                      |    -------     |
 *                      |     entry      |
 *                      |    -------     |
 *                      |     entry      |
 *                      |    -------     |
 *      allo_ent --->   |     .....      |
 *              |       |                |
 *              V       |                |
 *                      ~                ~
 *                      ~                ~
 *              ^       |                |
 *              |       |                |
 *      allo_item--->   |     ....       |
 *                      |    -------     |
 *                      |     item       |
 *                      |    -------     |
 *                      |     item       |
 *                      |    -------     |
 *                      |     item       |
 *                      +----------------+  <----- heap_end
 *
 * n_entry = allo_ent - table;
 */


#include "ndmlib.h"


int ndmfhh_initialize(struct ndmfhheap* fhh)
{
  NDMOS_MACRO_ZEROFILL(fhh);

  return NDMFHH_RET_OK;
}

int ndmfhh_commission(struct ndmfhheap* fhh, void* heap, unsigned size)
{
  fhh->heap_base = heap;
  fhh->heap_size = size;
  fhh->heap_end = (char*)heap + size;

  /* Align everything */
  fhh->heap_top = (void*)(((long)heap + (NDMOS_CONST_ALIGN - 1)) &
                          ~(NDMOS_CONST_ALIGN - 1));
  fhh->heap_bot =
      (void*)((long)((char*)heap + size) & ~(NDMOS_CONST_ALIGN - 1));

  ndmfhh_reset(fhh);

  return NDMFHH_RET_OK;
}

#define SLOP 32

int ndmfhh_prepare(struct ndmfhheap* fhh,
                   int fhtype,
                   int entry_size,
                   unsigned n_item,
                   unsigned total_size_of_items)
{
  void* pe;
  void* pi;
  unsigned items_need;

  if (fhh->heap_base == 0) return NDMFHH_RET_NO_HEAP;

  if (fhh->allo_entry == fhh->heap_top) {
    fhh->fhtype = fhtype;
    fhh->entry_size = entry_size;
  } else {
    if (fhh->fhtype != fhtype) return NDMFHH_RET_TYPE_CHANGE;

    if (fhh->entry_size != entry_size) return NDMFHH_RET_ENTRY_SIZE_MISMATCH;
  }

  items_need = total_size_of_items + n_item * NDMOS_CONST_ALIGN + SLOP;

  pe = (char*)fhh->allo_entry + fhh->entry_size;
  pi = (char*)fhh->allo_item - items_need;

  if (pe >= pi) return NDMFHH_RET_OVERFLOW;

  /* it'll fit! */
  return NDMFHH_RET_OK;
}

void* ndmfhh_add_entry(struct ndmfhheap* fhh)
{
  void* p;

  p = fhh->allo_entry;
  if ((char*)fhh->allo_entry + fhh->entry_size < (char*)fhh->allo_item) {
    fhh->allo_entry = (char*)p + fhh->entry_size;
    return p;
  } else {
    return 0;
  }
}

void* ndmfhh_add_item(struct ndmfhheap* fhh, unsigned size)
{
  void* p;

  size += (NDMOS_CONST_ALIGN - 1);
  size &= ~(NDMOS_CONST_ALIGN - 1);

  p = (char*)fhh->allo_item - size;
  if (p > fhh->allo_entry) {
    fhh->allo_item = p;
    return p;
  } else {
    return 0;
  }
}

void* ndmfhh_save_item(struct ndmfhheap* fhh, void* item, unsigned size)
{
  void* p;

  p = ndmfhh_add_item(fhh, size);
  if (p) { NDMOS_API_BCOPY(item, p, size); }
  return p;
}

int ndmfhh_reset(struct ndmfhheap* fhh)
{
  fhh->allo_entry = fhh->heap_top;
  fhh->allo_item = fhh->heap_bot;

  return NDMFHH_RET_OK;
}

int ndmfhh_get_table(struct ndmfhheap* fhh,
                     int* fhtype_p,
                     void** table_p,
                     unsigned* n_entry_p)
{
  unsigned n;

  *fhtype_p = fhh->fhtype;
  *table_p = fhh->heap_top;
  n = (char*)fhh->allo_entry - (char*)fhh->heap_top;
  if (n > 0) n /= fhh->entry_size;

  *n_entry_p = n;

  return NDMFHH_RET_OK;
}
