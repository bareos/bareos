# Documented the daemons state after installation

* Status: proposed
* Deciders: [list everyone involved in the decision]
* Date: 2025-11-25

Technical Story: From time to time we need to remember what is the expected state of daemons after install

## Context and Problem Statement

When doing package installation testing, or modification in packaging scripts it is not always evident to remember the **expected** state the daemon should have.
Previously we have a page somewhere in the internal wiki, which is obviously not accessible by contributor or bug reporter.

## Decision Drivers

* Have daemon status after installation clearly documented
* Make the status public so it can be found by anyone

## Considered Options

* Create formal ADR
* Keep internal wiki page
* Create discussion
* Create documentation page

## Decision Outcome

Chosen option: "[option 4]", because this fulfill the objective of documentation and normalization.

### Positive Consequences

* Make the expectation obvious for everyone at anytime
* Have a clear process in case we want to change or modify the state

### Negative Consequences

* ...

## Pros and Cons of the Options

### [option 1]

* Good, because it clarify publicly the expected state
* Good, ADR are typically made for this type of task/structure
* Bad, because it force use to work on and (re)define the state

### [option 2]

* Good, because there's no effort to provide
* Bad, because it keeps public information private

### [option 3]

* Good, because it open the debate in public
* Bad, because the tool doesn't offer versioning or follow-up

### [option 4]

* Good, because we already have documentation
* Bad, because we will have to repeat the information on several pages
* Bad, will force to create yet-another-page to have all the information grouped in one comparison table

