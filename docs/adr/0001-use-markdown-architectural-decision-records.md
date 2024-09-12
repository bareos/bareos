# Use Markdown Architectural Decision Records

Adapted from
[MADR's similar decision record](https://github.com/adr/madr/blob/2.1.2/docs/adr/0000-use-markdown-architectural-decision-records.md).

* Status: accepted
* Date: 2024-08-29

## Context and Problem Statement

We want to record architectural decisions made in this project.
Which format and structure should these records follow?

## Considered Options

* [MADR](https://adr.github.io/madr/) 2.1.2 - The Markdown Architectural Decision Records
* [Michael Nygard's template](http://thinkrelevance.com/blog/2011/11/15/documenting-architecture-decisions) - The first incarnation of the term "ADR"
* [Sustainable Architectural Decisions](https://www.infoq.com/articles/sustainable-architectural-design-decisions) - The Y-Statements
* Other templates listed at <https://github.com/joelparkerhenderson/architecture_decision_record>
* Formless - No conventions for file format and structure

## Decision Outcome

Chosen option: "MADR 2.1.2", because

* Implicit assumptions should be made explicit.
  Design documentation is important to enable people understanding the decisions later on.
  See also [A rational design process: How and why to fake it](https://doi.org/10.1109/TSE.1986.6312940).
* The MADR format is lean and fits our development style.
* The MADR structure is comprehensible and facilitates usage & maintenance.
* The MADR project is vivid.
* Version 2.1.2 is the latest one available when starting to document ADRs.

### Positive Consequences

The ADR are more structured. See especially:
* [MADR-0002 - Do not use numbers in headings](https://github.com/adr/madr/blob/2.1.2/docs/adr/0002-do-not-use-numbers-in-headings.md).
* [MADR-0005 - Use (unique number and) dashes in filenames](https://github.com/adr/madr/blob/2.1.2/docs/adr/0005-use-dashes-in-filenames.md).
* [MADR-0010 - Support categories (in form of subfolders with local ids)](https://github.com/adr/madr/blob/2.1.2/docs/adr/0010-support-categories.md).
* See [full set of MADR ADRs](https://github.com/adr/madr/blob/2.1.2/docs/adr).

### Negative Consequences

* Learning curve will be slightly longer.
