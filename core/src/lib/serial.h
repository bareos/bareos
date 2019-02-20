/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Written by John Walker, MM
 */
/**
 * @file
 *  Serialisation support functions from serial.c
 */


extern void serial_int16(uint8_t** const ptr, const int16_t v);
extern void serial_uint16(uint8_t** const ptr, const uint16_t v);
extern void serial_int32(uint8_t** const ptr, const int32_t v);
extern void serial_uint32(uint8_t** const ptr, const uint32_t v);
extern void serial_int64(uint8_t** ptr, int64_t v);
extern void serial_uint64(uint8_t** const ptr, const uint64_t v);
extern void SerialBtime(uint8_t** const ptr, const btime_t v);
extern void serial_float64(uint8_t** const ptr, const float64_t v);
extern void SerialString(uint8_t** const ptr, const char* const str);

extern int16_t unserial_int16(uint8_t** const ptr);
extern uint16_t unserial_uint16(uint8_t** const ptr);
extern int32_t unserial_int32(uint8_t** const ptr);
extern uint32_t unserial_uint32(uint8_t** const ptr);
extern int64_t unserial_int64(uint8_t** const ptr);
extern uint64_t unserial_uint64(uint8_t** const ptr);
extern btime_t UnserialBtime(uint8_t** const ptr);
extern float64_t unserial_float64(uint8_t** const ptr);
extern void UnserialString(uint8_t** const ptr, char* const str, int max);

/**

                         Serialisation Macros

    These macros use a uint8_t pointer, ser_ptr, which must be
    defined by the code which uses them.

*/

#ifndef BAREOS_LIB_SERIAL_H_
#define BAREOS_LIB_SERIAL_H_ 1

/*  ser_declare  --  Declare ser_ptr locally within a function.  */
#define ser_declare uint8_t* ser_ptr
#define unser_declare uint8_t* ser_ptr

/*  SerBegin(x, s)  --  Begin serialisation into a buffer x of size s.  */
#define SerBegin(x, s) ser_ptr = ((uint8_t*)(x))
#define UnserBegin(x, s) ser_ptr = ((uint8_t*)(x))

/*  SerLength  --  Determine length in bytes of serialised into a
                    buffer x.  */
#define SerLength(x) ((uint32_t)(ser_ptr - (uint8_t*)(x)))
#define UnserLength(x) ((uint32_t)(ser_ptr - (uint8_t*)(x)))

/*  SerEnd(x, s)  --  End serialisation into a buffer x of size s.  */
#define SerEnd(x, s) ASSERT(SerLength(x) <= (s))
#define UnserEnd(x, s) ASSERT(SerLength(x) <= (s))

/*  ser_check(x, s)  --  Verify length of serialised data in buffer x is
                         expected length s.  */
#define ser_check(x, s) ASSERT(SerLength(x) == (s))

/*                          Serialisation                   */

/*  8 bit signed integer  */
#define ser_int8(x) *ser_ptr++ = (x)
/*  8 bit unsigned integer  */
#define ser_uint8(x) *ser_ptr++ = (x)

/*  16 bit signed integer  */
#define ser_int16(x) serial_int16(&ser_ptr, x)
/*  16 bit unsigned integer  */
#define ser_uint16(x) serial_uint16(&ser_ptr, x)

/*  32 bit signed integer  */
#define ser_int32(x) serial_int32(&ser_ptr, x)
/*  32 bit unsigned integer  */
#define ser_uint32(x) serial_uint32(&ser_ptr, x)

/*  64 bit signed integer  */
#define ser_int64(x) serial_int64(&ser_ptr, x)
/*  64 bit unsigned integer  */
#define ser_uint64(x) serial_uint64(&ser_ptr, x)

/* btime -- 64 bit unsigned integer */
#define SerBtime(x) SerialBtime(&ser_ptr, x)


/*  64 bit IEEE floating point number  */
#define ser_float64(x) serial_float64(&ser_ptr, x)

/*  128 bit signed integer  */
#define ser_int128(x) \
  memcpy(ser_ptr, x, sizeof(int128_t)), ser_ptr += sizeof(int128_t)

/*  Binary byte stream len bytes not requiring serialisation  */
#define SerBytes(x, len) memcpy(ser_ptr, (x), (len)), ser_ptr += (len)

/*  Binary byte stream not requiring serialisation (length obtained by sizeof)
 */
#define ser_buffer(x) SerBytes((x), (sizeof(x)))

/* Binary string not requiring serialization */
#define SerString(x) SerialString(&ser_ptr, (x))

/*                         Unserialisation                  */

/*  8 bit signed integer  */
#define unser_int8(x) (x) = *ser_ptr++
/*  8 bit unsigned integer  */
#define unser_uint8(x) (x) = *ser_ptr++

/*  16 bit signed integer  */
#define unser_int16(x) (x) = unserial_int16(&ser_ptr)
/*  16 bit unsigned integer  */
#define unser_uint16(x) (x) = unserial_uint16(&ser_ptr)

/*  32 bit signed integer  */
#define unser_int32(x) (x) = unserial_int32(&ser_ptr)
/*  32 bit unsigned integer  */
#define unser_uint32(x) (x) = unserial_uint32(&ser_ptr)

/*  64 bit signed integer  */
#define unser_int64(x) (x) = unserial_int64(&ser_ptr)
/*  64 bit unsigned integer  */
#define unser_uint64(x) (x) = unserial_uint64(&ser_ptr)

/* btime -- 64 bit unsigned integer */
#define UnserBtime(x) (x) = UnserialBtime(&ser_ptr)

/*  64 bit IEEE floating point number  */
#define unser_float64(x) (x) = unserial_float64(&ser_ptr)

/*  128 bit signed integer  */
#define unser_int128(x) \
  memcpy(ser_ptr, x, sizeof(int128_t)), ser_ptr += sizeof(int128_t)

/*  Binary byte stream len bytes not requiring serialisation  */
#define UnserBytes(x, len) memcpy((x), ser_ptr, (len)), ser_ptr += (len)

/*  Binary byte stream not requiring serialisation (length obtained by sizeof)
 */
#define unser_buffer(x) UnserBytes((x), (sizeof(x)))

/* Binary string not requiring serialization (length obtained from max) */
#define unser_nstring(x, max) UnserialString(&ser_ptr, (x), (int)(max))

/*  Binary string not requiring serialisation (length obtained by sizeof)  */
#define UnserString(x) UnserialString(&ser_ptr, (x), sizeof(x))

#endif /* BAREOS_LIB_SERIAL_H_ */
