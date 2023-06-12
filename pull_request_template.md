### Thank you for contributing to the Bareos Project!

**Backport of PR #0000 to bareos-2x** (remove this line if this is no backport; for backport use cherry-pick -x)

#### Please check

- [ ] Short description and the purpose of this PR is present _above this paragraph_
- [ ] Your name is present in the AUTHORS file (optional)

If you have any questions or problems, please give a comment in the PR.

### Helpful documentation and best practices

- [Git Workflow](https://docs.bareos.org/DeveloperGuide/gitworkflow.html)
- [Automatic Sourcecode Formatting](https://docs.bareos.org/DeveloperGuide/generaldevel.html#automatic-sourcecode-formatting)
- [Check your commit messages](https://docs.bareos.org/DeveloperGuide/gitworkflow.html#commits)
- [Boy Scout Rule](https://docs.bareos.org/DeveloperGuide/generaldevel.html#boy-scout-rule)

### Checklist for the _reviewer_ of the PR (will be processed by the Bareos team)

##### General
- [ ] Is the PR title usable as CHANGELOG entry?
- [ ] Purpose of the PR is understood
- [ ] Commit descriptions are understandable and well formatted
- [ ] Check backport line
- [ ] Required backport PRs have been created

##### Source code quality

- [ ] Source code changes are understandable
- [ ] Variable and function names are meaningful
- [ ] Code comments are correct (logically and spelling)
- [ ] Required documentation changes are present and part of the PR
- [ ] `bareos-check-sources --since-merge` does not report any problems

##### Tests

- [ ] Decision taken that a test is required (if not, then remove this paragraph)
- [ ] The choice of the type of test (unit test or systemtest) is reasonable
- [ ] Testname matches exactly what is being tested
- [ ] On a fail, output of the test leads quickly to the origin of the fault
