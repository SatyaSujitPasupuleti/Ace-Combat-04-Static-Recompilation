# Contributing

Contributors must **write or update tests, review those tests, and run them** before submitting a change. Tests are part of the change, not a follow-up task.

## Required process

1. Describe the problem and observable acceptance criteria before implementing it.
2. Add meaningful unit tests for new or changed logic, including failure and boundary cases. Bug fixes need a regression test that demonstrates the original fault where practical.
3. Add or update acceptance tests for the affected user workflow. For launch/build changes, exercise the real command-line entry points with synthetic fixtures. For game/audio/rendering changes, also supply the relevant manual acceptance evidence described in [TESTING.md](TESTING.md).
4. Review both the implementation and its tests. Check that assertions verify required behavior rather than merely repeat the implementation, that fixtures exercise failures, and that tests do not hide regressions by weakening assertions or silently skipping coverage.
5. Run `build.cmd -Action Test` (or `python tools/run_tests.py --suite all`) after the final edits. Run any relevant native/runtime regression tests and build/manual checks too. Report the exact commands, results, environment, and untested areas in the pull request.
6. A reviewer must review the tests and their results as well as the code before merge. Fix failures and rerun affected checks; passing CI alone does not establish gameplay correctness.

Documentation-only changes should check links, commands, and accuracy; explain why executable tests are unchanged. If a test cannot run on your environment, explicitly report the limitation. Do not mark unrun tests as passed. Maintainers should obtain the missing validation before merging changes that depend on it.

## Test-data rules

Use synthetic inputs in public tests. Never commit or upload ISOs, game executables, generated game code/data, captured VU microcode, game captures, saves, credentials, or `local.json`. Game-based validation runs locally using your own ISO. Public CI must never require a game download or private assets.

Keep modifications focused. Preserve inherited licenses and credits. Do not enable AC5 address/layout assumptions for AC4 without evidence. Preserve the accepted movie/audio behavior and compare rendering/performance changes against the working native baseline.

The workflow checks public unit and launcher acceptance suites. Repository branch-protection rules are a separate maintainer configuration; their enforcement is not implied by this document.
