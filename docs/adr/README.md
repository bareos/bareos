# Bareos Architectural Decision Records

## Introduction
The Bareos project uses MADR (Markdown Architectural Decision Records) to record significant decisions made in the project.

For more details on ADRs in general see [BADR-0000](0000-record-architecture-decisions.md) and for MADRs see [BADR-0001](0001-use-markdown-architectural-decision-records.md).

All official Bareos ADRs are in the directory `docs/adr` inside the project's Git repository on the default branch.
## Workflows
### Creating a new ADR
To create a new ADR create a new markdown file based on the template in `docs/adr/template.md` . The required contents should be mostly self-documenting.
The initial *Status* should be **proposed**, but can also be set to **accepted** if the matter has already been discussed and agreed upon.
Use real names when listing *Deciders* and link to their GitHub profile.

The resulting file should be saved using the filename format `<NNNN>-<slug>.md` where
*  `<NNNN>` is the zero-padded consecutive four-digit number of the ADR or literal `XXXX` if you don't have a number yet
*  `<slug>` is the title of your ADR stripped of all non-alphanumeric characters in [`small-kebab-case`](https://www.theserverside.com/definition/Kebab-case)

That file should then be added to your Git branch in an individual commit.

### Accepting an ADR
When an ADR had been discussed and agreed upon, its *Status* will be changed to **accepted**. If the ADR isn't numbered yet, a number must be assigned and the name of the file must be updated.
Please double-check that the filename still matches the title of your ADR, because the title might have been changed during discussion.
The changes applied during this process should again result in an individual commit to your Git branch and the ADR will be published when your branch is merged into the default branch.

### Changing an accepted ADR
Once an ADR has been accepted, it should not be changed. If it is already on the default branch, it must not be changed.
When a decision is revised or changed, a new ADR that supersedes the previous one should be created.

### Superseding an ADR
When a new ADR supersedes one or more existing ADRs, there are two extra steps involved:
1. add a link to every superseded ADR to the *Links* section of the superseding ADR
2. change the *Status* of every superseded ADR to **superseded by `NNNN`**, preferably linking to the superseding ADR

### Linking to ADRs
Internal links between ADRs may use `BADR-<NNNN>` or `<NNNN>` as the link title. The plain filename should be used as a target. For example: `[BADR-0000](0000-record-architecture-decisions.md)`

External links to ADRs should use `BADR-<NNNN>` as the link title and the full URL to the file containing the ADR in the default branch of the repository as the target. This way users will always see the latest version of the ADR including supersedence information, if present.

### ADRs and backporting
When backporting changes that contain an ADR, the ADR should be handled just like every other piece of code or documentation. Even though there will be ADRs on release or feature branches the source of truth is still the default branch.

## Tool support
There are a few tools that can help managing ADRs. As of mid-2024 none of these is complete and most are not well maintained. You can try `pyadr` if you get tired of copying and editing manually, but our workflows don't require any tools.
