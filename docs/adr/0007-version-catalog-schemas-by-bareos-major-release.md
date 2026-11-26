# Version catalog schemas by Bareos major release

* Status: accepted
* Deciders: [Andreas Rogge](https://github.com/arogge), [Philipp Storz](https://github.com/pstorz)
* Date: 2026-11-12

Technical Story: [Issue 831](https://github.com/bareos/internal/issues/831), [discussion 827](https://github.com/bareos/internal/discussions/827)

## Context and Problem Statement

Catalog schemas can change while a Bareos major release is under development. If all such changes wait for a single release-time version update, installations running development packages cannot follow each schema change through an ordered migration path. Conversely, changing the catalog version for a software release that does not change the schema creates an unnecessary migration.

How should Bareos version catalog schema changes during development while retaining a simple upgrade path and a clear relationship to the target stable major release?

## Decision Drivers

* Allow development catalogs to upgrade through every schema change.
* Make the target Bareos major release evident from the catalog version.
* Keep migration ordering unambiguous across branches and releases.
* Avoid catalog version changes when the schema itself is unchanged.

## Considered Options

* Assign sequential versions within the target Bareos major's `NNxx` range, beginning at `NN00`.
* Assign one schema version per software release and defer development migrations.
* Keep a separate schema-version sequence unrelated to the target Bareos major.

## Decision Outcome

Chosen option: "Assign sequential versions within the target Bareos major's `NNxx` range."

The major number of the current Bareos development version determines the schema version's prefix. A schema change targeting Bareos 26 uses the 26xx range: the first such schema version is 2600, followed by 2601, 2602, and so on for subsequent schema changes in that major. Each schema-changing update gets a migration from the preceding catalog version, and fresh-catalog creation must report the current schema version.

For example:

| Development version | Preceding schema | Schema change | Resulting schema |
| --- | ---: | --- | ---: |
| `26.0.0~pre` | 2260 | Yes | 2600 |
| `26.0.0~pre` | 2600 | Yes | 2601 |
| `26.1.0~pre` | 2601 | Yes | 2602 |
| `27.0.0~pre` | 2602 | No | 2602 |
| `27.0.0~pre` | 2602 | Yes | 2700 |

The minor version and prerelease suffix do not determine the sequence number. A software release without a catalog schema change leaves the database version unchanged, even across a major boundary. When a schema change occurs at a major boundary, its migration upgrades directly from the latest preceding schema to the first schema version for the new major.

Adopt this numbering prospectively. The next schema change targeting Bareos 26 will transition existing version 2260 catalogs directly to 2600. This is a one-time transition from the legacy numbering scheme; do not rename or replay earlier released migrations. Adopting this policy alone leaves the catalog at version 2260.

### Positive Consequences

* Development installations can apply schema migrations in a deterministic order.
* The version identifies the Bareos major targeted by the schema change.
* Releases without catalog changes do not require a database migration.

### Negative Consequences

* Contributors must coordinate the next version within a target-major range to prevent two branches from assigning the same version.
* Moving between major-version ranges can create a numeric gap; the migration must still connect the actual preceding schema to the new version.

## Pros and Cons of the Options

### Assign sequential versions within the target Bareos major's `NNxx` range

* Good, because master-package users can upgrade after each schema change.
* Good, because versions communicate the target stable major.
* Bad, because schema-changing branches targeting the same major must coordinate version allocation.

### Assign one schema version per software release

* Good, because only one version is needed for each completed release.
* Bad, because development installations cannot reliably upgrade through individual schema changes.

### Keep a separate schema-version sequence

* Good, because schema versions are independent of software release numbering.
* Bad, because the version does not identify which Bareos major the schema targets.
* Bad, because contributors need to coordinate a second unrelated version sequence.

## Links

* [Issue 831](https://github.com/bareos/internal/issues/831)
* [Discussion 827](https://github.com/bareos/internal/discussions/827)
