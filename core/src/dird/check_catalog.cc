/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2019-2026 Bareos GmbH & Co. KG

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
#include "cats/cats.h"
#include "dird/check_catalog.h"
#include "dird/dird.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/ua_db.h"
#include "lib/parse_conf.h"

#include <charconv>
#include <string_view>

namespace directordaemon {

namespace {
struct SequenceInfo {
  std::string sequence_name;
  std::string table_name;
  std::string column_name;

  std::size_t next_value;
  std::size_t max_value;
};

bool ParseDbInt(std::string_view text, std::int64_t* out)
{
  if (text.size() == 0) { return false; }

  auto* start = text.data();
  auto* end = text.data() + text.size();

  auto result = std::from_chars(start, end, *out);
  if (result.ec != std::errc{} || result.ptr != end) { return false; }

  return true;
}

void CheckCatalogSequences(BareosDb* db,
                           CatalogUtility& util,
                           const CatalogResource* catalog)
{
  PoolMem query;
  db->FillQuery<BareosDb::SQL_QUERY::get_sequences>(query);


  std::vector<SequenceInfo> infos;
  run_query(
      db, query.c_str(),
      [&infos](const char* sequence_name, const char* table_name,
               const char* column_name, const char* next_value,
               const char* max_value) {
        if (!sequence_name) {
          throw std::runtime_error{"sequence-name is null"};
        }

        if (!table_name) { throw std::runtime_error{"table-name is null"}; }

        if (!column_name) { throw std::runtime_error{"column-name is null"}; }

        if (!next_value) { throw std::runtime_error{"next-value is null"}; }
        int64_t next_size{};
        if (!ParseDbInt(next_value, &next_size)) {
          throw std::runtime_error{"next-value could not be parsed as an int"};
        }

        if (!max_value) { throw std::runtime_error{"max-value is null"}; }
        int64_t max_size;
        if (!ParseDbInt(max_value, &max_size)) {
          throw std::runtime_error{"max-value could not be parsed as an int"};
        }

        if (next_size < 0 || max_size <= 0) {
          Dmsg0(200,
                "Got weird values for %s (%s:%s): next=%" PRId64 " max=%" PRId64
                "\n",
                sequence_name, table_name, column_name, next_size, max_size);
        } else {
          infos.emplace_back(sequence_name, table_name, column_name, next_size,
                             max_size);
        }
      });

  for (auto& info : infos) {
    if (info.max_value == 0) {
      // skip weird values
      Dmsg0(200, "Sequence %s for column %s.%s has maximum value 0",
            info.sequence_name.c_str(), info.table_name.c_str(),
            info.column_name.c_str());
      continue;
    }


    /* we use ~75% as our warning threshold, as that is easy to approximate
     * without relying on floats/linking libm.
     * Being precise here does not really matter, its ok to be a few % off,
     * as this is just a warning for the operator! */

    //                              ~50%             +         ~25%
    size_t warning_threshold = (info.max_value >> 1) + (info.max_value >> 2);

    if (info.next_value > warning_threshold) {
      util.warn(
          "Catalog \"%s\": Sequence %s for column %s.%s has almost reached its "
          "maximum (next: %zu, max: %zu)",
          catalog->resource_name_, info.sequence_name.c_str(),
          info.table_name.c_str(), info.column_name.c_str(), info.next_value,
          info.max_value);
    }
  }
}

bool check(CatalogUtility& util, BareosDb* db, const CatalogResource* catalog)
{
  /* Display a message if the db max_connections is too low */
  if (!db->CheckMaxConnections(NULL, me->MaxConcurrentJobs)) {
    util.warn("settings problem for Catalog=%s: %s", catalog->resource_name_,
              db->strerror());
  }

  try {
    CheckCatalogSequences(db, util, catalog);
  } catch (const std::exception& e) {
    util.warn(
        "Could not check all sequences in Catalog \"%s\", database \"%s\": %s",
        catalog->resource_name_, catalog->db_name, e.what());
  }


  return true;
}

bool update(CatalogUtility& util, BareosDb* db, const CatalogResource* catalog)
{
  /* Loop over all pools, defining/updating them in each database */
  PoolResource* pool;
  foreach_res (pool, R_POOL) {
    /* If the Pool has a catalog resource create the pool only
     *   in that catalog. */
    if (!pool->catalog || pool->catalog == catalog) {
      CreatePool(NULL, db, pool, POOL_OP_UPDATE); /* update request */
    }
  }

  /* Once they are created, we can loop over them again, updating
   * references (RecyclePool)
   */
  foreach_res (pool, R_POOL) {
    /* If the Pool has a catalog resource update the pool only
     *   in that catalog. */
    if (!pool->catalog || pool->catalog == catalog) {
      UpdatePoolReferences(NULL, db, pool);
    }
  }

  /* Ensure basic client record is in DB */
  ClientResource* client;
  foreach_res (client, R_CLIENT) {
    ClientDbRecord cr;
    /* Create clients only if they use the current catalog */
    if (client->catalog != catalog) {
      Dmsg3(500, "Skip client=%s with cat=%s not catalog=%s\n",
            client->resource_name_, client->catalog->resource_name_,
            catalog->resource_name_);
      continue;
    }
    Dmsg2(500, "create cat=%s for client=%s\n", client->catalog->resource_name_,
          client->resource_name_);
    bstrncpy(cr.Name, client->resource_name_, sizeof(cr.Name));
    db->CreateClientRecord(NULL, &cr);
  }

  /* Ensure basic storage record is in DB */
  StorageResource* store;
  foreach_res (store, R_STORAGE) {
    StorageDbRecord sr;
    MediaTypeDbRecord mtr;
    if (store->media_type) {
      bstrncpy(mtr.MediaType, store->media_type, sizeof(mtr.MediaType));
      mtr.ReadOnly = 0;
      db->CreateMediatypeRecord(NULL, &mtr);
    } else {
      mtr.MediaTypeId = 0;
    }
    bstrncpy(sr.Name, store->resource_name_, sizeof(sr.Name));
    sr.AutoChanger = store->autochanger;
    if (!db->CreateStorageRecord(NULL, &sr)) {
      util.err("Could not create storage record for %s", store->resource_name_);
      return false;
    }
    store->StorageId = sr.StorageId; /* set storage Id */
    if (!sr.created) {               /* if not created, update it */
      sr.AutoChanger = store->autochanger;
      if (!db->UpdateStorageRecord(NULL, &sr)) {
        util.err("Could not update storage record for %s",
                 store->resource_name_);
        return false;
      }
    }
  }

  /* Loop over all counters, defining them in each database */
  /* Set default value in all counters */
  CounterResource* counter;
  foreach_res (counter, R_COUNTER) {
    /* Write to catalog? */
    if (!counter->created && counter->Catalog == catalog) {
      CounterDbRecord cr;
      bstrncpy(cr.Counter, counter->resource_name_, sizeof(cr.Counter));
      cr.MinValue = counter->MinValue;
      cr.MaxValue = counter->MaxValue;
      cr.CurrentValue = counter->MinValue;
      if (counter->WrapCounter) {
        bstrncpy(cr.WrapCounter, counter->WrapCounter->resource_name_,
                 sizeof(cr.WrapCounter));
      } else {
        cr.WrapCounter[0] = 0; /* empty string */
      }
      if (db->CreateCounterRecord(NULL, &cr)) {
        counter->CurrentValue = cr.CurrentValue;
        counter->created = true;
        Dmsg2(100, "Create counter %s val=%d\n", counter->resource_name_,
              counter->CurrentValue);
      }
    }
    if (!counter->created) {
      counter->CurrentValue = counter->MinValue; /* default value */
    }
  }

  return true;
}

bool fix(CatalogUtility&, BareosDb* db)
{
  /* cleanup old job records */
  db->SqlQuery<BareosDb::SQL_QUERY::cleanup_created_job>();
  db->SqlQuery<BareosDb::SQL_QUERY::cleanup_running_job>();
  return true;
}

std::string format_as_string(const char* fmt, va_list args)
{
  std::string message;
  message.resize(128);

  va_list cpy;
  for (int i = 0; i < 3; ++i) {
    if (i == 2) {
      // something weird is going on with the formatting, just abort
      // this should never happen!
      Dmsg0(200,
            "formatting of '%s' failed: not successful after %d attempts\n",
            fmt, i);
      return "<could not be formatted>";
    }

    auto message_size = message.size();

    va_copy(cpy, args);
    // it is allowed to write 0 to message[message.size()], and vsnprintf
    // only ever writes 0 to the last place of a buffer
    int ret = vsnprintf(message.data(), message_size + 1, fmt, cpy);
    va_end(cpy);
    if (ret < 0) {
      // something went wrong, just continue
      Dmsg0(200, "formatting of '%s' failed: res = %d\n", fmt, ret);
      return "<could not be formatted>";
    }

    size_t content_bytes_written = static_cast<size_t>(ret);
    message.resize(content_bytes_written);
    if (content_bytes_written > message_size) {
      // the message did not fit
      continue;
    }

    break;
  }

  return message;
}

}  // namespace

void CatalogUtility::warn(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  auto message = format_as_string(fmt, args);
  va_end(args);
  warnings.emplace_back(std::move(message));
}

void CatalogUtility::err(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  auto message = format_as_string(fmt, args);
  va_end(args);
  error.emplace(std::move(message));
}

/**
 * In this routine,
 *  - we can check the connection (mode=CHECK_CONNECTION)
 *  - we can synchronize the catalog with the configuration
 * (mode=UPDATE_CATALOG)
 *  - we can synchronize, and fix old job records (mode=UPDATE_AND_FIX)
 */
bool CheckAllCatalogs(CatalogUtility& util)
{
  /* Loop over databases */
  CatalogResource* catalog;
  foreach_res (catalog, R_CATALOG) {
    BareosDb* db;

    /* Make sure we can open catalog, otherwise print a warning
     * message because the server is probably not running. */
    db = db_init_database(NULL, catalog->db_driver, catalog->db_name,
                          catalog->db_user, catalog->db_password.value,
                          catalog->db_address, catalog->db_port,
                          catalog->db_socket, catalog->mult_db_connections,
                          catalog->disable_batch_insert, catalog->try_reconnect,
                          catalog->exit_on_fatal, true);

    if (!db) {
      util.err("Could not open Catalog \"%s\", database \"%s\"",
               catalog->resource_name_, catalog->db_name);
      return false;
    }


    if (auto err = db->OpenDatabase()) {
      util.err("Could not open Catalog \"%s\", database \"%s\": %s",
               catalog->resource_name_, catalog->db_name, err);
      db->CloseDatabase(NULL);
      return false;
    }

    [&] {
      check(util, db, catalog);
      if (util.operation >= cat_op::UPDATE_CATALOG) {
        if (!update(util, db, catalog)) { return; }
      }
      if (util.operation >= cat_op::UPDATE_AND_FIX) {
        if (!fix(util, db)) { return; }
      }
    }();

    db->CloseDatabase(NULL);

    if (util.error) { return false; }
  }

  return true;
}

}  // namespace directordaemon
