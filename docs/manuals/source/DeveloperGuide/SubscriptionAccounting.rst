.. _developer-subscription-accounting:

Subscription Accounting
=======================

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
are Backup, Copy, Migrate, or Always-Incremental consolidation jobs (types
``B``, ``C``, ``g``, and ``O``). For each Client/FileSet, chain resolution
selects the latest completed Full, then the latest Differential after that
Full if one exists, then all Incrementals after the current Full or
Differential baseline. ``JobTDate`` orders jobs, with ``JobId`` as a
tie-breaker. A purged Full, Differential, or Incremental makes the chain
unusable; accounting does not silently sum only the remaining files. A tuple
with no usable chain is reported as excluded. A combination with no completed
Full is not discovered by the full-chain scan and therefore has no accounting
row.

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
yet valid. After an eligible completed backup, copy, migrate, or consolidation
job (status ``T`` or ``W``), the Director queues that JobId. The worker
re-resolves the current chain, so a queued job that is no longer the latest
chain member does not overwrite a newer result.

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

Coverage is split between a small formatting unit test and the
Director/system-test fixtures. In the parent task, nine focused unit tests
passed. The expanded basic accounting integration, status-subscriptions,
bpipe, and Virtual Full integrations also passed (21 CTest entries,
including setup and cleanup); the final three basic accounting cases were
rerun after the last additions and passed. These integrations ran on Linux.
Catalog fixtures exercise Windows-specific metadata and NDMP catalog shapes,
but no live Windows client or NDMP server was tested.

.. list-table::
   :header-rows: 1
   :widths: 28 48 24

   * - Behavior
     - Test source
     - Status
   * - Rule codes, table layout, hidden logical column, excluded-row rendering;
       NDMP size/block conversion and invalid-size handling
     - :file:`core/src/tests/test_subscription_accounting_table.cc`
       and :file:`core/src/tests/ndmp_fhdb_helpers_test.cc`
     - Nine focused unit tests passed
   * - POSIX byte accounting, sub-512-byte block rounding, non-regular entries,
       Full/Incremental newest-version selection, report JSON and logical
       display
     - :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions-accounting`
     - Passed in the expanded Linux integration; final cases also passed on
       rerun
   * - Windows logical-size selection; MSSQL VDI versus a real empty file;
       opaque-image fallback and missing-size exclusion; NDMP_BAREOS and
       NDMP_NATIVE with and without file history; imported catalog samples
     - :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions-accounting`
       and :file:`systemtests/tests/bareos-basic/fixtures/README.rst`
     - Linux catalog-fixture assertions passed, including the final
       native-NDMP zero-size case. Fixture imports do not run a live Windows
       or NDMP server
   * - Refresh worker status, snapshot metadata, legacy unit-report parity,
       and JSON output
     - :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions`
     - Passed
   * - Corrected bpipe file attributes, A/L rule reporting, logical-size
       display, and Virtual Full preservation
     - :file:`systemtests/tests/bpipe-fd/testrunner-environment-variables-and-accounting`
     - Passed
   * - Virtual Full accounting and catalog attribute preservation
     - :file:`systemtests/tests/virtualfull-basic/testrunner-simple-and-accounting`
     - Passed
   * - Live NDMP no-file-history estimate
     - :file:`systemtests/tests/ndmp-bareos/testrunner-09-status-subscriptions-accounting`
     - Hardware-dependent and not run here; no live NDMP-server run is
       claimed
   * - POSIX sparse file and hard-link names, plus Accurate deletion markers
       in a real backup chain and delta-versus-full-recalculation comparison
     - :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions-accounting`
     - Passed on Linux, including the final rerun
   * - Differential chain selection, eligible job types, no usable chain,
       ignored failed Full and migrated-job rows, same-timestamp JobId
       tie-break, and Full/Differential/Incremental purge handling
     - :file:`systemtests/tests/bareos-basic/accounting_edge_fixture.py`
       imported by :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions-accounting`
     - Passed on Linux, including the final rerun
   * - Native NDMP without history and zero baseline size; synthetic NDMP
       archive without history and zero baseline size
     - :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions-accounting`
     - Passed on Linux, including the final rerun
   * - An ordinary empty Full backup with nonzero JobBytes remains measured
       as zero allocated bytes (rule A), rather than using rule J
     - :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions-accounting`
     - Passed on Linux, including the final rerun
   * - Invalid ``st_size``/``st_blocks``, multiplication overflow, and
       per-tuple byte/logical-total overflow; prior snapshot and timing
       metadata remain intact after failure
     - :file:`systemtests/tests/bareos-basic/testrunner-status-subscriptions-accounting`
     - Passed on Linux, including the final rerun. ``st_blocks`` and
       allocated-byte overflow cases are POSIX-only; invalid ``st_size`` and
       logical-total overflow checks are cross-platform
   * - Hyper-V estimated Full/Incremental chain
     - :file:`systemtests/tests/bareos-basic/plugin_accounting_fixture.py`
       and :file:`systemtests/tests/bareos-basic/fixtures/README.rst`
     - Both phases imported and two-job chain assertion passed on Linux
   * - A tuple with Incremental rows but no completed Full
     - :file:`systemtests/tests/bareos-basic/accounting_edge_fixture.py`
     - Explicitly asserted absent from the report; passed on Linux

When changing chain selection or stat interpretation, keep fixture expectations
derived from the catalog rows and decoded attributes rather than hard-coding
filesystem allocation assumptions. The catalog fixtures are regression data,
not evidence that a live platform-specific client or NDMP server was exercised.
