# Understand the machinery

* [Harness code map](code-map.md) - Where each build, scoring, selection and policing job lives; which pieces are comparison-class; where the tests are.
* [Policy hook (mg64-policy)](policy-hook.md) - The omp tool-call guard: what bash, read, write, edit and ast_edit calls it blocks, how paths are resolved, and its known limits.
* [Git pre-commit hook and CI](git-hook-and-ci.md) - The local pre-commit rebuild, the GitHub Actions ROM build, and how to build and publish the private container that bakes in the ROM.

# Change it

* [Changing the harness](changing-the-harness.md) - Workflow changes are free; comparison changes need a failing-first test and before/after evidence; protected paths never change; commit harness changes separately.
