/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
/*
 * edit.c  edit string to ascii, and ascii to internal
 *
 * Kern Sibbald, December MMII
 */

#include "include/bareos.h"
#include "lib/edit.h"
#include <math.h>
#include <cstdlib>
#include <algorithm>

// We assume ASCII input and don't worry about overflow
uint64_t str_to_uint64(const char* str)
{
  const char* p = str;
  uint64_t value = 0;

  if (!p) { return 0; }

  while (B_ISSPACE(*p)) { p++; }

  if (*p == '+') { p++; }

  while (B_ISDIGIT(*p)) {
    value = B_TIMES10(value) + *p - '0';
    p++;
  }

  return value;
}

int64_t str_to_int64(const char* str)
{
  const char* p = str;
  int64_t value;
  bool negative = false;

  if (!p) { return 0; }

  while (B_ISSPACE(*p)) { p++; }

  if (*p == '+') {
    p++;
  } else if (*p == '-') {
    negative = true;
    p++;
  }

  value = str_to_uint64(p);
  if (negative) { value = -value; }

  return value;
}

/*
 * Edit an integer number with commas, the supplied buffer must be at least
 * min_buffer_size bytes long. The incoming number is always widened to 64
 * bits.
 */
char* edit_uint64_with_commas(uint64_t val, char* buf)
{
  edit_uint64(val, buf);

  return add_commas(buf, buf);
}

/*
 * Edit an integer into "human-readable" format with four or fewer significant
 * digits followed by a suffix that indicates the scale factor. The buf array
 * inherits a min_buffer_size byte minimum length requirement from
 * edit_unit64_with_commas(), although the output string is limited to eight
 * characters.
 */
char* edit_uint64_with_suffix(uint64_t val, char* buf)
{
  int commas = 0;
  char *c, mbuf[50];
  static const char* suffix[]
      = {"", "K", "M", "G", "T", "P", "E", "Z", "Y", "FIX ME"};
  int suffixes = sizeof(suffix) / sizeof(*suffix);

  edit_uint64_with_commas(val, mbuf);
  if ((c = strchr(mbuf, ',')) != NULL) {
    commas++;
    *c++ = '.';
    while ((c = strchr(c, ',')) != NULL) {
      commas++;
      *c++ = '\0';
    }
    mbuf[5] = '\0'; /* Drop this to get '123.456 TB' rather than '123.4 TB' */
  }

  if (commas >= suffixes) { commas = suffixes - 1; }
  Bsnprintf(buf, edit::min_buffer_size, "%s %s", mbuf, suffix[commas]);

  return buf;
}

/*
 * Edit an integer number, the supplied buffer must be at least
 * min_buffer_size bytes long. The incoming number is always widened to 64
 * bits. Replacement for sprintf(buf, "%" llu, val)
 */
char* edit_uint64(uint64_t val, char* buf)
{
  char mbuf[50];
  mbuf[sizeof(mbuf) - 1] = 0;
  int i = sizeof(mbuf) - 2; /* Edit backward */

  if (val == 0) {
    mbuf[i--] = '0';
  } else {
    while (val != 0) {
      mbuf[i--] = "0123456789"[val % 10];
      val /= 10;
    }
  }
  bstrncpy(buf, &mbuf[i + 1], edit::min_buffer_size);

  return buf;
}

/*
 * Edit an integer number, the supplied buffer must be at least
 * min_buffer_size bytes long. The incoming number is always widened to 64
 * bits. Replacement for sprintf(buf, "%" llu, val)
 */
char* edit_int64(int64_t val, char* buf)
{
  if (val == 0) {
    bstrncpy(buf, "0", edit::min_buffer_size);
    return buf;
  } else if (val == std::numeric_limits<int64_t>::min()) {
    bstrncpy(buf, "-9223372036854775808", edit::min_buffer_size);
    return buf;
  }

  char mbuf[50];
  bool negative = false;
  mbuf[sizeof(mbuf) - 1] = 0;
  int i = sizeof(mbuf) - 2; /* Edit backward */

  if (val < 0) {
    negative = true;
    val = -val;
  }
  while (val != 0) {
    mbuf[i--] = "0123456789"[val % 10];
    val /= 10;
  }
  if (negative) { mbuf[i--] = '-'; }
  bstrncpy(buf, &mbuf[i + 1], edit::min_buffer_size);

  return buf;
}

/*
 * Edit an integer number with commas, the supplied buffer must be at least
 * min_buffer_size bytes long. The incoming number is always widened to 64
 * bits.
 */
char* edit_int64_with_commas(int64_t val, char* buf)
{
  edit_int64(val, buf);

  return add_commas(buf, buf);
}

/*
 * Given a string "str", separate the numeric part into str, and the modifier
 * into mod.
 */
struct modifier_parse_result {
  double number;
  std::string_view modifier;
  const char* rest;
};

static std::string_view TrimLeft(std::string_view input)
{
  while (input.size() > 0 && B_ISSPACE(input.front())) {
    input.remove_prefix(1);
  }
  return input;
}

static std::optional<modifier_parse_result> GetModifier(const char* input)
{
  Dmsg2(900, "parsing \"%s\"\n", input);
  char* num_end;
  errno = 0;
  // strtod takes care of leading space
  auto number = strtod(input, &num_end);
  if (number == 0 && errno != 0) {
    Dmsg0(900, "parse error: \"%s\" ERR=%s\n", input, strerror(errno));
    return std::nullopt;
  }

  if (num_end == input) {
    // we do not accept empty inputs (but strtod does!)
    Dmsg0(900, "parse error: \"%s\" ERR=no number\n", input);
    return std::nullopt;
  }

  std::string_view rest{num_end};

  auto trimmed = TrimLeft(rest);

  auto mod_end = std::find_if(trimmed.begin(), trimmed.end(), [](auto c) {
    return B_ISDIGIT(c) || B_ISSPACE(c);
  });
  auto mod = trimmed.substr(0, mod_end - trimmed.begin());

  const char* rest_input = mod.data() + (mod.end() - mod.begin());

  Dmsg2(900, "num=%lf mod=\"%.*s\" rest=\"%s\"\n", number, (int)mod.size(),
        mod.data(), rest_input);
  // empty mod is ok, so no need to check!
  return modifier_parse_result{number, mod, rest_input};
}

// mults needs to be at least as big as mods; mods needs to be NULL terminated.
// returns the number as well as the rest of the string that was not parsed.
static std::pair<std::uint64_t, const char*> parse_number_with_mod(
    const char* str,
    const char* const mods[],
    const double mults[])
{
  std::uint64_t total = 0;

  while (*str) {
    double number;
    std::string_view modifier;
    const char* rest;
    if (auto res = GetModifier(str); !res) {
      return {total, str};
    } else {
      number = res->number;
      modifier = res->modifier;
      rest = res->rest;
    }

    if (modifier.size() == 0) {
      total += static_cast<std::uint64_t>(number);
    } else {
      bool found = false;
      for (int i = 0; mods[i]; ++i) {
        if (bstrncasecmp(modifier.data(), mods[i], modifier.size())) {
          // without the static_cast, this will first cast total to double,
          // then add the doubles, and then finally cast back to uint64,
          // which we do not want!  doubles lose precision to quickly:
          // 1 exabyte + 1 == 1 exabyte for doubles
          total += static_cast<std::uint64_t>(number * mults[i]);
          found = true;
          break;
        }
      }
      if (!found) {
        Dmsg1(900, "Unknown modifier: \"%.*s\"\n",
              static_cast<int>(modifier.size()), modifier.data());
        return {total, str};
      }
    }

    str = rest;
  }

  return {total, str};
}

// returns wether the string only contains junk characters
static bool IsJunk(const char* str)
{
  for (auto* head = str; *head; ++head) {
    if (!b_isjunkchar(*head)) { return false; }
  }

  return true;
}

/*
 * Convert a string duration to utime_t (64 bit seconds)
 * Returns false: if error
 *         true:  if OK, and value stored in value
 */
bool DurationToUtime(const char* str, utime_t* value)
{
  // The "n" = mins and months appears before minutes so that m maps to months.
  static const char* mod[]
      = {"n",    "seconds", "months",   "minutes", "mins",     "hours",
         "days", "weeks",   "quarters", "years",   (char*)NULL};
  static const double mult[] = {60,
                                1,
                                60 * 60 * 24 * 30,
                                60,
                                60,
                                3600,
                                3600 * 24,
                                3600 * 24 * 7,
                                3600 * 24 * 91,
                                3600 * 24 * 365,
                                0};

  if (auto [total, rest] = parse_number_with_mod(str, mod, mult);
      IsJunk(rest)) {
    *value = static_cast<utime_t>(total);
    return true;
  } else {
    return false;
  }
}

// Edit a utime "duration" into ASCII
char* edit_utime(utime_t val, char* buf, int buf_len)
{
  char mybuf[200];
  static constexpr utime_t mult[]
      = {60 * 60 * 24 * 365, 60 * 60 * 24 * 30, 60 * 60 * 24, 60 * 60, 60};
  static const char* mod[] = {"year", "month", "day", "hour", "min"};

  *buf = 0;
  for (int i = 0; i < 5; i++) {
    utime_t times = val / mult[i];
    if (times > 0) {
      val = val - times * mult[i];
      Bsnprintf(mybuf, sizeof(mybuf), "%" PRIi64 " %s%s ", times, mod[i],
                times > 1 ? "s" : "");
      bstrncat(buf, mybuf, buf_len);
    }
  }

  if (val == 0 && strlen(buf) == 0) {
    bstrncat(buf, "0 secs", buf_len);
  } else if (val != 0) {
    Bsnprintf(mybuf, sizeof(mybuf), "%d sec%s", (uint32_t)val,
              val > 1 ? "s" : "");
    bstrncat(buf, mybuf, buf_len);
  }

  return buf;
}

char* edit_pthread(pthread_t val, char* buf, int buf_len)
{
  int i;
  char mybuf[3];
  unsigned char* ptc = (unsigned char*)(void*)(&val);

  bstrncpy(buf, "0x", buf_len);
  for (i = sizeof(val); i; --i) {
    Bsnprintf(mybuf, sizeof(mybuf), "%02x", (unsigned)(ptc[i - 1]));
    bstrncat(buf, mybuf, buf_len);
  }

  return buf;
}

static bool strunit_to_uint64(const char* str,
                              uint64_t* value,
                              const char** mod)
{
  static const double mult[] = {
      1,     // Byte
      1024,  // KiB Kibibyte
      1000,  // kB Kilobyte

      1048576,  // MiB Mebibyte
      1000000,  // MB Megabyte

      1073741824,  // GiB Gibibyte
      1000000000,  // GB Gigabyte

      1099511627776,  // TiB Tebibyte
      1000000000000,  // TB Terabyte

      1125899906842624,  // PiB Pebibyte
      1000000000000000,  // PB Petabyte

      1152921504606846976,  // EiB Exbibyte
      1000000000000000000,  // EB Exabyte
                            // 18446744073709551616 2^64

  };

  if (auto [total, rest] = parse_number_with_mod(str, mod, mult);
      IsJunk(rest)) {
    *value = total;
    return true;
  } else {
    return false;
  }
}

// convert uint64 number to size string
std::string SizeAsSiPrefixFormat(uint64_t value_in)
{
  uint64_t value = value_in;
  int factor;
  std::string result{};
  // convert default value string to numeric value
  static const char* modifier[]
      = {" e", " p", " t", " g", " m", " k", "", NULL};
  const uint64_t multiplier[] = {1152921504606846976,  // EiB Exbibyte
                                 1125899906842624,     // PiB Pebibyte
                                 1099511627776,        // TiB Tebibyte
                                 1073741824,           // GiB Gibibyte
                                 1048576,              // MiB Mebibyte
                                 1024,                 // KiB Kibibyte
                                 1};

  if (value == 0) {
    result += "0";
  } else {
    for (int t = 0; modifier[t] && (value > 0); t++) {
      factor = value / multiplier[t];
      value = value % multiplier[t];
      if (factor > 0) {
        result += std::to_string(factor);
        result += modifier[t];
        if (value > 0) { result += " "; }
      }
    }
  }
  return result;
}


/*
 * Convert a size in bytes to uint64_t
 * Returns false: if error
 *         true:  if OK, and value stored in value
 */
bool size_to_uint64(const char* str, uint64_t* value)
{
  // not used
  // clang-format off
  static const char* mod[] = {"*",
                              // kibibyte, kilobyte
                              "k",  "kb",
                              // mebibyte, megabyte
                              "m",  "mb",
                              // gibibyte, gigabyte
                              "g",  "gb",
                              // tebibyte, terabyte
                              "t",  "tb",
                              // pebibyte, petabyte
                              "p",  "pb",
                              // exbibyte, exabyte
                              "e",  "eb",
                              NULL};

  // clang-format on
  return strunit_to_uint64(str, value, mod);
}

/*
 * Convert a speed in bytes/s to uint64_t
 * Returns false: if error
 *         true:  if OK, and value stored in value
 */
bool speed_to_uint64(const char* str, uint64_t* value)
{
  // not used
  static const char* mod[] = {"*", "k/s", "kb/s", "m/s", "mb/s", NULL};

  return strunit_to_uint64(str, value, mod);
}

/*
 * Check if specified string is a number or not.
 * Taken from SQLite, cool, thanks.
 */
bool Is_a_number(const char* n)
{
  bool digit_seen = false;

  if (*n == '-' || *n == '+') { n++; }

  while (B_ISDIGIT(*n)) {
    digit_seen = true;
    n++;
  }

  if (digit_seen && *n == '.') {
    n++;
    while (B_ISDIGIT(*n)) { n++; }
  }

  if (digit_seen && (*n == 'e' || *n == 'E')
      && (B_ISDIGIT(n[1])
          || ((n[1] == '-' || n[1] == '+') && B_ISDIGIT(n[2])))) {
    n += 2; /* Skip e- or e+ or e digit */
    while (B_ISDIGIT(*n)) { n++; }
  }

  return digit_seen && *n == 0;
}

// Check if specified string is a list of numbers or not
bool Is_a_number_list(const char* n)
{
  bool previous_digit = false;
  bool digit_seen = false;

  while (*n) {
    if (B_ISDIGIT(*n)) {
      previous_digit = true;
      digit_seen = true;
    } else if (*n == ',' && previous_digit) {
      previous_digit = false;
    } else {
      return false;
    }
    n++;
  }

  return digit_seen && *n == 0;
}

// Check if the specified string is an integer
bool IsAnInteger(std::string_view v)
{
  if (v.empty()) { return false; }

  auto found = std::find_if(std::begin(v), std::end(v),
                            [](char c) { return !B_ISDIGIT(c); });
  return found == std::end(v);
}

/*
 * Check if BAREOS Resource Name is valid
 *
 * If ua is non-NULL send the message
 */
bool IsNameValid(const char* name, std::string& msg)
{
  int len;
  const char* p;
  // Special characters to accept
  const char* accept = ":.-_/ ";

  /* Empty name is invalid */
  if (!name) {
    msg = "Empty name not allowed.\n";
    return false;
  }

  /* check for beginning space */
  if (name[0] == ' ') {
    msg = "Name cannot start with space.\n";
    return false;
  }

  // Restrict the characters permitted in the Volume name
  for (p = name; *p; p++) {
    if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
      continue;
    }
    msg = "Illegal character \"";
    msg += *p;
    msg.append("\" in name.\n");
    return false;
  }

  len = p - name;
  if (len >= MAX_NAME_LENGTH) {
    msg = "Name too long.\n";
    return false;
  }

  if (len == 0) {
    msg = "Name must be at least one character long.\n";
    return false;
  } else {
    /* check for ending space */
    if (*(p - 1) == ' ') {
      msg = "Name cannot end with space.\n";
      return false;
    }
  }

  return true;
}

bool IsNameValid(const char* name)
{
  bool retval;
  std::string msg{};

  retval = IsNameValid(name, msg);

  return retval;
}

// Add commas to a string, which is presumably a number.
char* add_commas(const char* val, char* buf)
{
  auto in_length = strlen(val);
  // we add a comma every three characters, so we have breakpoints at 4, 7, ...
  auto num_commas = in_length ? (in_length - 1) / 3 : 0;

  if (num_commas == 0) {
    memmove(buf, val, in_length + 1);
    return buf;
  }

  auto out_length = in_length + num_commas;
  buf[out_length] = '\0';

  // we need to compute where to insert the first comma
  auto comma_offset = (3 - (in_length % 3)) % 3;

  // we need to copy in reverse, in case val and buf overlap
  for (int i = in_length - 1; i >= 0; --i) {
    // we need to move the output by how many commas exist before that
    // position!
    auto prev_commas = (i + comma_offset) / 3;
    buf[i + prev_commas] = val[i];
  }

  for (size_t i = 0; i < num_commas; ++i) {
    buf[i * 4 + 3 - comma_offset] = ',';
  }

  return buf;
}

/*
 * check if acl entry is valid
 * valid acl entries contain only A-Z 0-9 and !*.:_-'/
 */
bool IsAclEntryValid(const char* acl, PoolMem& msg)
{
  int len;
  const char* p;
  const char* accept = "!()[]|+?*.:_-'/"; /* Special characters to accept */

  if (!acl) {
    Mmsg(msg, T_("Empty acl not allowed.\n"));
    return false;
  }

  /* Restrict the characters permitted in acl */
  for (p = acl; *p; p++) {
    if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
      continue;
    }
    Mmsg(msg, T_("Illegal character \"%c\" in acl.\n"), *p);
    return false;
  }

  len = p - acl;
  if (len >= MAX_NAME_LENGTH) {
    Mmsg(msg, T_("Acl too long.\n"));
    return false;
  }

  if (len == 0) {
    Mmsg(msg, T_("Acl must be at least one character long.\n"));
    return false;
  }

  return true;
}

bool IsAclEntryValid(const char* acl)
{
  PoolMem msg;
  return IsAclEntryValid(acl, msg);
}
