# Always use integer when comparing timestamps numbers in Python

* Status: accepted
* Deciders: Bruno Friedmann, Sebastian Sura
* Date: 2024-12-03

Technical Story: [#1982]

## Context and Problem Statement

When comparing WAL file timestamps for incremental backups in the PostgreSQL python plugin,
the file timestamp coming from `os.stat()` function is expressed in float format, while the
`last_backup_time` in `ROP` record is a simple integer. As floats are architecture and language
dependent they are source of free interpretation and might lead to strange results when compared
to integers.
We were also using function `int()` to round them, which often lead the plugin to backup again
previous archived wal file.

## Decision Drivers

* comparing different types or using conversion functions (e.g. rounding) are prone to error.
* comparing floats is only valid with strict greater or lower than. equal can't be used safely
* integers can be as large as necessary, and are safe to compare even cross-architecture

## Considered Options

* Compare file, job timestamp as integer (nanoseconds)
* Compare file, job timestamp as float (microsecond)
* Don't change the code; mix of int, float, datetime representation

## Decision Outcome

Chosen option: "Compare file, job timestamp as integer (nanoseconds)", because comes out best
(see below).

### Positive Consequences

* always using integers will ensure we get trustful comparisons.
* we will always backup the files that we have to backup and no more.

### Negative Consequences

* file time stamps are not time zone aware, they are always using the local time stamp.
* datetime (in Python) still does not support nanoseconds.

## Pros and Cons of the Options

### Compare file, job timestamp as integer (nanoseconds)

* Good, because integer are safe in comparison whatever architecture is used.
* Good, because integer are large in all case.
* Good, because we can drop datetime datetimeutil import.
* Good, because offering smooth upgrade with previous version of the plugin.
* Bad, because integer with nanoseconds will force (in python) to check if _ns function are present,
  and force to do a division to retrieve the microsecond granularity.
* Bad, because it will need some efforts to standardize the code.

### Compare file, job timestamp as float (microsecond)

* Good, because float are easily transformable as `datetime` / `time` type.
* Good, because float are the native format of `os.stat()` function without checking if nanoseconds
  are supported.
* Bad, because their representation is architecture dependent.
* Bad, because their representation can't be compared with equality (only greater or lower).

### Don't change the code; mix of int, float, datetime representation

* Good, because the software will behave exactly as it is actually.
* Good, because it offer freedom in implementation.
* Bad, because it create deviant behavior depending of the platform and language used.
