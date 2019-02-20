/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
/*

                   Serialisation Support Functions
                          John Walker
*/

#include "include/bareos.h"
#include "serial.h"

/*

        NOTE:  The following functions should work on any
               vaguely contemporary platform.  Production
               builds should use optimised macros (void
               on platforms with network byte order and IEEE
               floating point format as native.

*/

/*  serial_int16  --  Serialise a signed 16 bit integer.  */

void serial_int16(uint8_t** const ptr, const int16_t v)
{
  int16_t vo = htons(v);

  memcpy(*ptr, &vo, sizeof vo);
  *ptr += sizeof vo;
}

/*  serial_uint16  --  Serialise an unsigned 16 bit integer.  */

void serial_uint16(uint8_t** const ptr, const uint16_t v)
{
  uint16_t vo = htons(v);

  memcpy(*ptr, &vo, sizeof vo);
  *ptr += sizeof vo;
}

/*  serial_int32  --  Serialise a signed 32 bit integer.  */

void serial_int32(uint8_t** const ptr, const int32_t v)
{
  int32_t vo = htonl(v);

  memcpy(*ptr, &vo, sizeof vo);
  *ptr += sizeof vo;
}

/*  serial_uint32  --  Serialise an unsigned 32 bit integer.  */

void serial_uint32(uint8_t** const ptr, const uint32_t v)
{
  uint32_t vo = htonl(v);

  memcpy(*ptr, &vo, sizeof vo);
  *ptr += sizeof vo;
}

/*  serial_int64  --  Serialise a signed 64 bit integer.  */

void serial_int64(uint8_t** const ptr, const int64_t v)
{
  if (bigendian()) {
    memcpy(*ptr, &v, sizeof(int64_t));
  } else {
    int i;
    uint8_t rv[sizeof(int64_t)];
    uint8_t* pv = (uint8_t*)&v;

    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(*ptr, &rv, sizeof(int64_t));
  }
  *ptr += sizeof(int64_t);
}


/*  serial_uint64  --  Serialise an unsigned 64 bit integer.  */

void serial_uint64(uint8_t** const ptr, const uint64_t v)
{
  if (bigendian()) {
    memcpy(*ptr, &v, sizeof(uint64_t));
  } else {
    int i;
    uint8_t rv[sizeof(uint64_t)];
    uint8_t* pv = (uint8_t*)&v;

    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(*ptr, &rv, sizeof(uint64_t));
  }
  *ptr += sizeof(uint64_t);
}


/*  SerialBtime  --  Serialise an btime_t 64 bit integer.  */

void SerialBtime(uint8_t** const ptr, const btime_t v)
{
  if (bigendian()) {
    memcpy(*ptr, &v, sizeof(btime_t));
  } else {
    int i;
    uint8_t rv[sizeof(btime_t)];
    uint8_t* pv = (uint8_t*)&v;

    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(*ptr, &rv, sizeof(btime_t));
  }
  *ptr += sizeof(btime_t);
}


/*  serial_float64  --  Serialise a 64 bit IEEE floating point number.
                        This code assumes that the host floating point
                        format is IEEE and that floating point quantities
                        are stored in IEEE format either LSB first or MSB
                        first.  More creative host formats will require
                        additional transformations here.  */

void serial_float64(uint8_t** const ptr, const float64_t v)
{
  if (bigendian()) {
    memcpy(*ptr, &v, sizeof(float64_t));
  } else {
    int i;
    uint8_t rv[sizeof(float64_t)];
    uint8_t* pv = (uint8_t*)&v;

    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(*ptr, &rv, sizeof(float64_t));
  }
  *ptr += sizeof(float64_t);
}

void SerialString(uint8_t** const ptr, const char* const str)
{
  int i;
  char* dest = (char*)*ptr;
  char* src = (char*)str;
  for (i = 0; src[i] != 0; i++) { dest[i] = src[i]; }
  dest[i++] = 0; /* Terminate output string */
  *ptr += i;     /* update pointer */
  // Dmsg2(000, "ser src=%s dest=%s\n", src, dest);
}


/*  unserial_int16  --  Unserialise a signed 16 bit integer.  */

int16_t unserial_int16(uint8_t** const ptr)
{
  int16_t vo;

  memcpy(&vo, *ptr, sizeof vo);
  *ptr += sizeof vo;
  return ntohs(vo);
}

/*  unserial_uint16  --  Unserialise an unsigned 16 bit integer.  */

uint16_t unserial_uint16(uint8_t** const ptr)
{
  uint16_t vo;

  memcpy(&vo, *ptr, sizeof vo);
  *ptr += sizeof vo;
  return ntohs(vo);
}

/*  unserial_int32  --  Unserialise a signed 32 bit integer.  */

int32_t unserial_int32(uint8_t** const ptr)
{
  int32_t vo;

  memcpy(&vo, *ptr, sizeof vo);
  *ptr += sizeof vo;
  return ntohl(vo);
}

/*  unserial_uint32  --  Unserialise an unsigned 32 bit integer.  */

uint32_t unserial_uint32(uint8_t** const ptr)
{
  uint32_t vo;

  memcpy(&vo, *ptr, sizeof vo);
  *ptr += sizeof vo;
  return ntohl(vo);
}

/*  unserial_uint64  --  Unserialise an unsigned 64 bit integer.  */

uint64_t unserial_uint64(uint8_t** const ptr)
{
  uint64_t v;

  if (bigendian()) {
    memcpy(&v, *ptr, sizeof(uint64_t));
  } else {
    int i;
    uint8_t rv[sizeof(uint64_t)];
    uint8_t* pv = (uint8_t*)&v;

    memcpy(&v, *ptr, sizeof(uint64_t));
    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(&v, &rv, sizeof(uint64_t));
  }
  *ptr += sizeof(uint64_t);
  return v;
}

/*  UnserialBtime  --  Unserialise a btime_t 64 bit integer.  */

btime_t UnserialBtime(uint8_t** const ptr)
{
  btime_t v;

  if (bigendian()) {
    memcpy(&v, *ptr, sizeof(btime_t));
  } else {
    int i;
    uint8_t rv[sizeof(btime_t)];
    uint8_t* pv = (uint8_t*)&v;

    memcpy(&v, *ptr, sizeof(btime_t));
    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(&v, &rv, sizeof(btime_t));
  }
  *ptr += sizeof(btime_t);
  return v;
}


/*  unserial_float64  --  Unserialise a 64 bit IEEE floating point number.
                         This code assumes that the host floating point
                         format is IEEE and that floating point quantities
                         are stored in IEEE format either LSB first or MSB
                         first.  More creative host formats will require
                         additional transformations here.  */

float64_t unserial_float64(uint8_t** const ptr)
{
  float64_t v;

  if (bigendian()) {
    memcpy(&v, *ptr, sizeof(float64_t));
  } else {
    int i;
    uint8_t rv[sizeof(float64_t)];
    uint8_t* pv = (uint8_t*)&v;

    memcpy(&v, *ptr, sizeof(float64_t));
    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(&v, &rv, sizeof(float64_t));
  }
  *ptr += sizeof(float64_t);
  return v;
}

void UnserialString(uint8_t** const ptr, char* const str, int max)
{
  int i;
  char* src = (char*)(*ptr);
  char* dest = str;
  for (i = 0; i < max && src[i] != 0; i++) { dest[i] = src[i]; }
  dest[i++] = 0; /* Terminate output string */
  *ptr += i;     /* update pointer */
  // Dmsg2(000, "unser src=%s dest=%s\n", src, dest);
}
