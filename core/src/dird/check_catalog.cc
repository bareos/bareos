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

constexpr char kCatalogSequenceWarningQuery[] = R"sql(
WITH sequences AS (
    SELECT
        seq.relname AS sequence_name,
        table_.relname AS table_name,
        attribute.attname AS column_name,
        CASE
            WHEN pg_sequence_last_value(sequence_data.seqrelid) IS NULL
                THEN sequence_data.seqstart
            ELSE pg_sequence_last_value(sequence_data.seqrelid)
                 + sequence_data.seqincrement
        END AS next_value,
        least(
            sequence_data.seqmax,
            CASE attribute.atttypid
                WHEN 'int4'::regtype THEN 2147483647::bigint
                WHEN 'int8'::regtype THEN 9223372036854775807::bigint
            END) AS max_value
    FROM pg_sequence AS sequence_data
    JOIN pg_class AS seq ON seq.oid = sequence_data.seqrelid
    JOIN pg_depend AS dependency
      ON dependency.objid = sequence_data.seqrelid
     AND dependency.classid = 'pg_class'::regclass
     AND dependency.refclassid = 'pg_class'::regclass
     AND dependency.deptype = 'a'
    JOIN pg_class AS table_ ON table_.oid = dependency.refobjid
    JOIN pg_attribute AS attribute
      ON attribute.attrelid = table_.oid
     AND attribute.attnum = dependency.refobjsubid
    WHERE table_.relnamespace = 'public'::regnamespace
      AND table_.relkind IN ('r', 'p')
      AND attribute.atttypid IN ('int4'::regtype, 'int8'::regtype)
      AND sequence_data.seqincrement > 0
      AND NOT sequence_data.seqcycle
)
SELECT sequence_name, table_name, column_name, next_value, max_value
FROM sequences
ORDER BY next_value::numeric / max_value DESC;
)sql";

struct CatalogSequenceWarningContext {
  const CatalogResource* catalog;
  bool invalid_result = false;
};

int AddCatalogSequenceWarning(void* ctx, int fields, char** row)
{
  auto* context = static_cast<CatalogSequenceWarningContext*>(ctx);
  if (fields != 5 || !row[0] || !row[1] || !row[2] || !row[3] || !row[4]) {
    context->invalid_result = true;
    return 1;
  }

  uint64_t next_value;
  uint64_t maximum_value;
  const auto [next_end, next_error] = std::from_chars(
      row[3], row[3] + std::string_view(row[3]).size(), next_value);
  const auto [maximum_end, maximum_error] = std::from_chars(
      row[4], row[4] + std::string_view(row[4]).size(), maximum_value);
  if (next_error != std::errc{} || *next_end != '\0'
      || maximum_error != std::errc{} || *maximum_end != '\0') {
    context->invalid_result = true;
    return 1;
  }

  if (!CatalogSequenceAtWarningThreshold(next_value, maximum_value)) {
    return 0;
  }

  PoolMem warning(PM_MESSAGE);
  Mmsg(warning,
       T_("Catalog \"%s\": sequence \"%s\" for %s.%s will reach its maximum "
          "value soon (next value: %s, maximum value: %s)."),
       context->catalog->resource_name_, row[0], row[1], row[2], row[3],
       row[4]);
  my_config->AddWarning(warning.c_str());
  return 0;
}

void CheckCatalogSequences(BareosDb* db, const CatalogResource* catalog)
{
  CatalogSequenceWarningContext context{catalog};
  if (!db->SqlQuery(kCatalogSequenceWarningQuery, AddCatalogSequenceWarning,
                    &context)) {
    Jmsg(nullptr, M_WARNING, 0,
         T_("Could not check sequences in Catalog \"%s\", database \"%s\": "
            "%s\n"),
         catalog->resource_name_, catalog->db_name, db->strerror());
  } else if (context.invalid_result) {
    Jmsg(nullptr, M_WARNING, 0,
         T_("Could not interpret sequence information in Catalog \"%s\", "
            "database \"%s\".\n"),
         catalog->resource_name_, catalog->db_name);
  }
}

}  // namespace

/**
 * In this routine,
 *  - we can check the connection (mode=CHECK_CONNECTION)
 *  - we can synchronize the catalog with the configuration
 * (mode=UPDATE_CATALOG)
 *  - we can synchronize, and fix old job records (mode=UPDATE_AND_FIX)
 */
bool CheckCatalog(cat_op mode)
{
  bool OK = true;

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
      Pmsg2(000, T_("Could not open Catalog \"%s\", database \"%s\".\n"),
            catalog->resource_name_, catalog->db_name);
      Jmsg(NULL, M_FATAL, 0,
           T_("Could not open Catalog \"%s\", database \"%s\".\n"),
           catalog->resource_name_, catalog->db_name);
      OK = false;
      goto bail_out;
    }


    if (auto err = db->OpenDatabase()) {
      Pmsg2(000, T_("Could not open Catalog \"%s\", database \"%s\": %s\n"),
            catalog->resource_name_, catalog->db_name, err);
      Jmsg(NULL, M_FATAL, 0,
           T_("Could not open Catalog \"%s\", database \"%s\": %s\n"),
           catalog->resource_name_, catalog->db_name, err);
      db->CloseDatabase(NULL);
      OK = false;
      goto bail_out;
    }

    /* Display a message if the db max_connections is too low */
    if (!db->CheckMaxConnections(NULL, me->MaxConcurrentJobs)) {
      Pmsg1(000, "Warning, settings problem for Catalog=%s\n",
            catalog->resource_name_);
      Pmsg1(000, "%s", db->strerror());
    }

    /* we are in testing mode, so don't touch anything in the catalog */
    if (mode == CHECK_CONNECTION) {
      db->CloseDatabase(NULL);
      continue;
    }

    CheckCatalogSequences(db, catalog);

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
      Dmsg2(500, "create cat=%s for client=%s\n",
            client->catalog->resource_name_, client->resource_name_);
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
        Jmsg(NULL, M_FATAL, 0, T_("Could not create storage record for %s\n"),
             store->resource_name_);
        OK = false;
        goto bail_out;
      }
      store->StorageId = sr.StorageId; /* set storage Id */
      if (!sr.created) {               /* if not created, update it */
        sr.AutoChanger = store->autochanger;
        if (!db->UpdateStorageRecord(NULL, &sr)) {
          Jmsg(NULL, M_FATAL, 0, T_("Could not update storage record for %s\n"),
               store->resource_name_);
          OK = false;
          goto bail_out;
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
    /* cleanup old job records */
    if (mode == UPDATE_AND_FIX) {
      db->SqlQuery<BareosDb::SQL_QUERY::cleanup_created_job>();
      db->SqlQuery<BareosDb::SQL_QUERY::cleanup_running_job>();
    }

    /* Set type in global for debugging */
    SetDbType(db->GetType());

    db->CloseDatabase(NULL);
  }

bail_out:
  return OK;
}

}  // namespace directordaemon
