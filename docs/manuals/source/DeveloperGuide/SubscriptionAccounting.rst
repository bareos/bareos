.. _developer-subscription-accounting:

Subscription Accounting
=======================

The goal of subscription accounting is to determine the amount of primary
data backed up from the original systems, not the accumulated backup
history, transferred bytes, or space occupied on backup storage. Catalog
measurements are used where available; job-size estimates are a fallback
when source-data measurements are unavailable. These estimates are proxies
for primary-data size, not direct measurements of it.

Accounting deduplicates file versions within each Client/FileSet chain,
but does not globally deduplicate overlapping data backed up through
different Client/FileSet combinations.

This chapter describes the Director's catalog-based subscription accounting
implementation behind :bcommand:`status subscriptions`. It is intended for
contributors changing the calculation, catalog persistence, or report. The
measured accounting report is distinct from subscription-unit policy: it
provides per-Client/FileSet file and byte totals, while the existing unit
report retains its plugin grouping and count- versus volume-based rules.

Implementation overview
-----------------------

The Director calculates a cumulative result for each Client/FileSet from the
catalog's ``File.LStat`` attributes. The calculation is in
:file:`core/src/dird/ua_acct_calc.cc`; worker scheduling and the report are in
:file:`core/src/dird/ua_acct.cc`; loading and persistence are in
:file:`core/src/dird/ua_acct_snapshot.cc`. The accounting report is appended
to the existing Director status response; it does not ask a File Daemon or a
plugin to calculate or report these values.

The result is a snapshot, not a sum of job transfer statistics. A regular
file that is unchanged across several jobs contributes once, using its newest
catalog version in the selected chain. The stored result on a Job row is the
cumulative result for that Job's Client/FileSet chain.

Eligible chain jobs have a successful or warning status (``T`` or ``W``) and
are file-bearing Backup or Copy result jobs (types ``B`` and ``C``).
Migration and Always-Incremental consolidation control jobs (types ``g`` and
``O``) are excluded: their resulting Backup or Copy jobs carry the file rows
used for accounting. For each Client/FileSet name, chain resolution uses the
FileSet version belonging to its latest completed Full, then selects the
latest Differential after that Full if one exists, followed by all
Incrementals after the current Full or Differential baseline.
``JobTDate`` orders jobs, with ``JobId`` as a tie-breaker. A purged Full,
Differential, or Incremental makes the chain unusable; accounting does not
silently sum only the remaining files. A tuple with no usable chain is
reported as excluded. A combination with no completed Full is not discovered
by the full-chain scan and therefore has no accounting row.

The catalog query selects the newest File row for each ``(PathId, Name)``
across the selected chain (latest ``JobTDate``, then ``JobId``). This
deduplicates file versions without keeping the whole file set in Director
memory. It is path/name deduplication, not inode deduplication: separate names
for a hard-linked file each count as a file. Directories, symlinks, and other
non-regular entries are not counted. An Accurate-mode deletion marker
(``FileIndex = 0``) suppresses the previous version and adds no file or bytes.
Without Accurate deletion records, a file removed from the client can remain
in the chain until a later Full or Differential replaces the baseline.

Measured and estimated sizes
----------------------------

Each measured regular-file row contributes its attributes according to the
Client platform:

* **A** (``st_blocks*512``): allocated bytes on non-Windows clients.
  ``st_blocks`` is a count of 512-byte units; this is not the filesystem's
  reported ``st_blksize``.
* **L** (``st_size``): logical file length. Windows clients use this value for
  both the accounted and logical totals because their ``st_blocks`` value is
  synthetic. Client platform detection uses the reported ``Client.Uname``.
* **J** (``job_bytes``): estimate from the baseline Full job, used when the
  chain has an opaque image or is an NDMP backup without file history. The
  estimate is ``max(ReadBytes, JobBytes)``; the file count is the baseline
  Full's ``JobFiles``. Since this is not a file-level measurement, the logical
  total repeats the estimate rather than representing independently measured
  ``st_size`` values.

The opaque-image marker used by Barri and some virtual-machine plugins is
recognized from its synthetic stat attributes, not from plugin names. MSSQL
VDI streams are recognized under ``/@MSSQL/`` using their zero-size VDI
placeholder attributes. Either kind makes the whole Client/FileSet tuple use
rule J rather than adding the marker to otherwise measurable File rows. A
genuinely empty regular file remains measurable.

NDMP has two catalog shapes. NDMP_BAREOS can record a synthetic archive
container under ``/@NDMP/`` alongside file-history rows. When measurable
history exists, the container is ignored and the file rows determine the
result. If there is no measurable file history, the baseline Full job is used
as rule J; if its byte count is unavailable or zero, the tuple is excluded
with ``ndmp_no_file_history``. NDMP_NATIVE does not create that synthetic
archive File row. When its file history is absent, a saved
``NDMPJobEnvironment`` record identifies it as NDMP, allowing the same
baseline-Full estimate or exclusion. With file history, native NDMP is
measured from the regular File rows. Thus both formats have the same outcomes
with and without history, despite their different catalog markers.

An unrecognized, negative, or overflowing stat field needed by the selected
rule, an addition overflow, or a catalog query failure aborts calculation.
It is not converted into a smaller total. Excluded tuples are retained in the
report with unavailable values and a reason; they are not silently omitted.
Their values do not enter the grand totals, and the response marks totals
incomplete.

Incremental maintenance and persistence
---------------------------------------

At Director startup, the worker queues latest per-tuple results that are not
yet valid. After a successful or warning Backup or Copy result job (status
``T`` or ``W``), the Director queues that JobId. These include file-bearing
results produced by migration and consolidation workflows; the migration and
consolidation control jobs themselves are not queued. The worker re-resolves
the current chain, so a queued job that is no longer the latest chain member
does not overwrite a newer result.

When a FileSet is recreated with the same name, its new catalog ID represents
the current version for accounting. The newest accountable Full selects that
version. Until a result for the new version is available, snapshot loading may
continue to show the previous valid result for the same Client/FileSet name;
once the new result is saved, the older version is no longer selected.

When possible, an incremental calculation starts from the previous cumulative
result and applies the current job's changed File rows: remove the prior
effective version and add the current version. If the previous result is
missing, excluded, uses a different measurement rule, or contains an opaque
plugin stream, the worker recomputes the full chain. Discovering an opaque
stream while applying a delta also triggers a full scan, since the fallback
estimate depends on the complete chain. The explicit recalculation described
below always scans every Client/FileSet chain.

Per-Job accounting values and validity are stored on the ``Job`` row. The
``SubscriptionAccountingSnapshot`` singleton stores the latest attempt,
successful snapshot timestamp and error, plus the completion timestamp and
duration of the last successful full recalculation. Saving calculated rows
and advancing the snapshot's success timestamp is transactional; a failed
save rolls back the row updates, preserving the last successful values. A
calculation or save failure is recorded separately and does not change the
backup Job's result. Full-recalculation timing metadata is written after the
snapshot save succeeds. Its measured duration excludes queue wait and covers
calculation and saving the accounting rows.

The worker owns a catalog connection and processes queued work in the
background. Only the first configured Catalog is refreshed; multiple catalogs
are not supported by this feature. The worker stops and is joined during
Director shutdown or configuration reload. A running scan checks for
cancellation between rows. Status output reports worker availability, queued
work, and running work, and continues to show the last successful snapshot
while a refresh is pending or has failed.

An operator can request a full recalculation with
:bcommand:`status subscriptions recalculate`. It is asynchronous; duplicate
requests while a full refresh is queued or running are coalesced. The command
requires the ``status`` command permission and unrestricted access to Client,
Job, and FileSet resources. It cannot be combined with report filters or
modifiers. A running scan cannot be cancelled from the console.

The lightweight :bcommand:`status subscriptions revision` command returns
the worker state and an in-memory generation number without loading the report
rows. The generation advances when accounting work is queued and when a worker
calculation completes, including a failed calculation. Clients can poll this
command and fetch the full report only when the generation or worker state
changes. The generation is not persisted and resets when the Director
restarts, so a polling client must fetch a full report to establish its
baseline after connecting or entering the subscription view.

Corrected plugin attributes are significant to this calculation because they
contain the final file size and allocation. When a plugin resends corrected
attributes after saving a file, the resend must use the same effective file
options as the original save, including sparse and offset options. The
Storage Daemon and its tools retain only the latest Unix attribute record for
each FileIndex when applying or displaying file metadata. In particular,
``bls`` lists the corrected attributes once, and ``bscan`` flushes pending
attributes before applying a restore object for the same job session so that
the restore object is associated with the correct FileId.

Reporting and compatibility
---------------------------

The default :bcommand:`status subscriptions` response and its ``all`` variant
include both the existing unit report and measured accounting details.
Accounting byte totals do not redefine subscription-unit counting or volume
policy; the unit sections continue to use their existing grouping and
count-based rules. The optional ``logical`` modifier changes the human-readable output only.
Structured output already includes ``logical_bytes`` on each
measured or estimated row and ``total_logical_bytes`` in the summary, so the
modifier does not change the JSON fields or report shape.

The text table uses A, L, and J as short rule codes. Structured rows include
the rule name, rule code, estimate flag, chain job count, and excluded status
and reason where applicable. In particular, rule J is explicitly an
estimate; it must not be described as measured allocated or logical file
size. The subscription-unit report continues to expose its existing
count-/volume-based decision separately.

For the user-facing options, examples, and report limitations, see
:ref:`bareos-console-subscription-accounting`.

Test coverage
-------------

The tests cover three distinct layers: isolated helpers, calculations against
catalog fixtures, and accounting after actual backup operations. Catalog
fixtures exercise platform-specific metadata without requiring the original
plugin or system. They do not replace end-to-end testing of those components.

The table identifies CTest names, including GoogleTest suites registered with
the ``gtest:`` prefix (``*`` denotes all cases in a suite). Vue entries use
their Vitest suite names.

.. list-table::
   :header-rows: 1
   :widths: 30 45 25

   * - Contract verified
     - Test name
     - Test method and prerequisites
   * - A/L/J codes, table alignment, optional logical column and issue column
     - ``gtest:SubscriptionAccountingTable.*``
     - Unit tests; no running daemons or catalog
   * - NDMP file-history size conversion, block rounding and rejection of
       invalid or out-of-range sizes
     - ``gtest:ndmp_fhdb_helpers.*``
     - Unit tests; no NDMP server
   * - Negative stat fields remain invalid when decoded into unsigned
       Windows stat members
     - ``gtest:stat_utils.compile_time_checks``
     - Compile-time checks for signed and unsigned representations
   * - File counts and sizes, exclusion of non-regular entries, newest-file
       selection across Full/Incremental jobs, optional logical display and
       unchanged structured fields
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Actual backups with running daemons and PostgreSQL; expected
       allocation is derived from backed-up catalog attributes. Windows
       runs check rule L and exact source-byte totals after Full and Incremental
   * - Sparse allocation, counting hard-linked names, Accurate deletion and
       agreement between per-job updates and full recalculation
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Actual POSIX filesystem backups; these cases are omitted on Windows
   * - Windows logical-size selection; Barri, MSSQL VDI, VMware and Hyper-V
       job-size estimates; Full/Incremental plugin chains
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Imported catalog samples, checked by the basic accounting runner;
       no live Windows client or plugin required
   * - NDMP_BAREOS and NDMP_NATIVE file-history totals and newest-file
       selection across Full/Incremental jobs
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Imported catalog exports, checked by the basic accounting runner;
       no NDMP server required
   * - NDMP_BAREOS and NDMP_NATIVE without history: baseline-Full estimate,
       or exclusion when the baseline has no usable size
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Synthetic archive rows and NDMP environment metadata in PostgreSQL
   * - Opaque-image and MSSQL placeholder recognition, measured empty files,
       missing-size exclusions and incomplete totals
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Controlled catalog attribute changes
   * - Differential chain selection, eligible job types, rejection of failed
       and superseded migrated jobs, JobId tie-breaking, purged chains,
       empty Fulls and combinations without a completed Full
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Synthetic catalog chains, checked by the basic accounting runner
   * - Invalid sizes and arithmetic overflow abort recalculation without
       replacing the last valid snapshot or successful timing metadata
     - ``system:bareos-basic:status-subscriptions-accounting``
     - Controlled catalog attribute changes; block-count and allocated-byte
       overflow cases are POSIX-only
   * - Snapshot provenance, stale and failed-refresh reporting, legacy
       unit-report parity, filtering and anonymization
     - ``system:bareos-basic:status-subscriptions``
     - PostgreSQL report fixtures with running daemons
   * - Accounting schema additions do not prevent historical job records
       from being written; repeated statistics updates do not duplicate jobs
     - ``system:random-commands:start``
     - Actual Full, Differential and Incremental backups followed by
       statistics updates and a catalog history check
   * - Corrected bpipe attributes reach the catalog; accounting survives
       Virtual Full consolidation
     - ``system:bpipe-fd:environment-variables-and-accounting``
     - Actual plugin backups, restore and consolidation
   * - Virtual Full preserves accounted totals and catalog attributes
     - ``system:virtualfull-basic:simple-and-accounting``
     - Actual backup and consolidation jobs
   * - Always Incremental accounting matches source file counts and logical
       sizes; consolidation preserves totals, including Accurate deletions;
       automatic updates agree with full recalculation
     - ``system:always-incremental-consolidate:01-full+incr``;
       ``system:always-incremental-consolidate:05-consolidate-deleted-file``
     - Actual backups and consolidation; requires the Always Incremental
       suite to be enabled
   * - Live NDMP file-history accounting and no-history job-size estimate
     - ``system:ndmp-bareos:09-status-subscriptions-accounting``
     - Requires a configured NDMP data agent and the preceding NDMP suite jobs
   * - Logical-column selection, refresh behavior, warnings, baseline-job
       navigation events and normal/anonymized PDF content
     - ``Director subscription calculation refresh``;
       ``Subscription snapshot age and refresh eligibility``
     - Vue component tests with mocked Director responses

When changing chain selection or stat interpretation, keep fixture expectations
derived from the catalog rows and decoded attributes rather than hard-coding
filesystem allocation assumptions. See
:file:`systemtests/tests/bareos-basic/fixtures/README.rst` for the provenance
and expected totals of imported samples. Test results belong in CI reports,
not in this coverage inventory.
