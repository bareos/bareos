# Bareos Developer Tools

This directory contains tools for working with the Bareos source code. As a user, you will likely never need them.

## Tool Installation

### Pip Tools
1. Navigate to the `devtools/` directory.
2. Run `install-pip-tools.sh`.
3. Once installed, you should be able to run the pip-tools from any directory.
4. Test the installation by running `bareos-check-sources` from outside the `devtools/` directory.

If step 2 fails, ensure that the environment variable `$PATH` includes a directory that is writable by the current user.

>**Note:**
>In order to be able to use the `pr-tool`, you require the `gh` GitHub CLI tool.
>Install it and login (using `gh auth`) to your GitHub account you have your pull requests on. 
---

### PHP CS Fixer (for WebUI)
Ensure that:
- `composer` is installed.

1. Navigate to the `devtools/` directory.
2. Run `php-cs-fixer/install-php-cs-fixer.sh`.

## Tool Usage

### Check Sources

#### Bareos Check Sources
Ensure that:
- Pip-Tools are properly installed.
- You are inside a Git repository.

Run `bareos-check-sources` to check your uncommitted files for the following requirements:
- `clang-format`
- `cmake-format`
- Python `black`
- Trailing spaces
- Trailing newlines
- DOS line endings
- Missing copyright notices
- Incorrect copyright end years
- Merge conflicts
- Executables missing a shebang
- Python executables missing a proper shebang
- C/C++ header guards
- Comments wasting space

Options:
- `--since-merge` – Additionally check all files that were part of a commit since the last merge.
- `--diff` – Output the difference between your source and the requested changes.
- `--modify` – Automatically apply the requested changes to the checked files.

---

#### Format PHP Source (for WebUI)
Ensure that `php-cs-fixer` is installed as described above.

Run `php-cs-fixer/run-php-cs-fixer.sh` to:
- Scan all PHP files in the WebUI folder for rule violations according to [PSR-12](https://www.php-fig.org/psr/psr-12/).
- If a file violates the rules, apply necessary fixes and stop.

> **NOTE**: Only one file is fixed at a time.

---

### PR Tool
The PR Tool provides various workflows to simplify PR development, merging, and backporting.

#### Check
Verify which merge requirements for your PR are met.

Ensure that:
- Pip-Tools and the GitHub CLI are properly [set up](#pip-tools)
- Your local branch's upstream branch is part of a pull request.

Run:
```shell
pr-tool check
```
This command checks the following:
- PR state is OPEN.
- Local and PR head commits match.
- PR is mergeable.
- PR is not a draft.
- All requested review changes have been addressed.
- All checkboxes in the PR description are ticked.
- No uncommitted changes exist in the repository.
- Commit format checks pass.
- `bareos-check-sources --since-merge` succeeds.

#### Add Changelog
> **NOTE**: When merging, the changelog is updated automatically. This command is rarely needed manually.

Ensure that:
- Pip-Tools and the GitHub CLI are properly [set up](#pip-tools).
- There are no uncommitted changes.
- Your local branch's upstream branch is part of a pull request.

Run:
```shell
pr-tool add-changelog
```
This command adds an entry for the current pull request in the changelog.

---

#### Update License
Ensure that:
- Pip-Tools are properly [set up](#pip-tools).

Run:
```shell
pr-tool update-license
```
This updates the `LICENSE.txt` file at the root of the Bareos repository.

---

#### Merge
Ensure that:
- Pip-Tools and the GitHub CLI are properly [set up](#pip-tools).
- There are no uncommitted changes.
- Your local branch's upstream branch is part of a pull request.

Run:
```shell
pr-tool merge
```
This command:
- Checks if the pull request is mergeable.
  > Use `--ignore-status-checks` to bypass status checks (i.e. Jenkins).
- Adds an entry for the current pull request in the changelog.
- Pushes the changes.
- Merges the pull request.
  > Use `--admin-override` to apply `--admin` privileges when merging.

Skipping the last step is possible with `--skip-merge`.

#### Dump
This command outputs the fetched data of the pull request associated with the local branch.

Ensure that:
- Pip-Tools and the GitHub CLI are properly [set up](#pip-tools).
- Your local branch's upstream branch is part of a pull request.

Run:
```shell
pr-tool dump
```

---

#### Backport
Create a backport branch and PR based on an existing PR.

Ensure that:
- Pip-Tools and the GitHub CLI are properly [set up](#pip-tools).
- Your local branch's upstream branch is part of a pull request.
- The remote your upstream branch is on is a ssh connection (not https)

Steps:
1. Checkout the branch you want to backport to (e.g., `bareos-24`).
2. Run:
   ```shell
   pr-tool backport create <pr-number>
   ```
   to backport the specified PR.
3. Verify the new local backport branch.
4. Run:
   ```shell
   pr-tool backport publish
   ```
   push the local branch and create a PR for the backport branch.

---

### Release

#### Prepare Release
This script handles the preparation and tagging of release commits and base commits for future work.

Ensure that:
- You are on the branch you want to release.
  > For a major release, use the release branch, not `master`.
- You have no uncommitted changes.

Run:
```shell
prepare-release.sh [<version>]
```
This performs the following:
1. Creates a release commit.
2. Cleans up after the release commit.
3. Sets up a new base for future work (not for pre-releases).
4. Creates tags for the release and WIP state (if applicable).

Before pushing, ensure that you are on the correct branch and review the commits, tags, and branch pointers.

Verify external documentation links using:
```shell
sphinx-build -M linkcheck docs/manuals/source out/ -j2
```
or (outside the docbuild container):
```shell
make docs-check-urls
```

#### New Changelog Release
> **Note:** When preparing a release, a new changelog release is automatically added.

Run:
- `new-changelog-release.sh unreleased` to add an "Unreleased" section.
- `new-changelog-release.sh <version> <date>` to add a new changelog section with the provided version and date.

#### Update Changelog Links
> **Note:** Changelog links are automatically updated when preparing a release.

Run:
```shell
update-changelog-links.sh
```
to update links in `CHANGELOG.md`.

---

### Build

#### Build and Test with Sanitize
Ensure that:
- You are at the root of the Bareos repository.

Run:
```shell
build-and-test-with-sanitize.sh
```
to build and test with sanitization enabled.

#### Build RPM
Build a RPM package of the entire Git repository.

Ensure that:
- The required RPM tools are properly installed.

Run:
```shell
build-rpm.sh
```

Options:
- `--ulc` – Package to `bareos-universal-client` instead of `bareos`.
- `--tarball` – Create a tarball.
- `--fast-tarball` – Create a tarball with `--fast` option.

---

#### Build Tarball
Create a tarball of the entire Git repository.

Ensure that:
- You are inside a Git repository.
- The repository has no uncommitted changes.
- `xz` is installed (for compression).

Run:
```shell
dist-tarball.sh [options] [<directory>]
```
This creates a tarball and stores it under `<directory>` (or `/tmp` if not given).

Options:
- `--fast` – Uses fast compression.
- `--best` – Uses best compression.
