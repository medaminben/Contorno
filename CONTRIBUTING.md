# Contributing to Contorno

Thanks for helping out. Bug reports, ideas and pull requests are all welcome.

Contorno is not open source: the code is published under an all rights
reserved [licence](LICENSE). By opening a pull request you agree that the
copyright holder may use, change and distribute your contribution as part of
Contorno, under any licence, and you confirm that you have the right to give
that permission.

## Reporting bugs and asking for features

Open an [issue](https://github.com/medaminben/Contorno/issues/new/choose) and pick the bug report
or feature request form. For a wrong area, attach the photo (or say where it
can be fetched) and the set-up values: distance, focal length, sensor or pixel
size.

Do not report security problems in a public issue. [SECURITY.md](SECURITY.md) explains how to report
them privately.

## Making a change

1. Fork the repository and create a branch from `Beta`.
2. Build and run the tests:

   ```bash
   cmake --preset debug
   cmake --build --preset debug --target isnet_model   # once
   cmake --build --preset debug
   ctest --preset debug
   ```

   The window tests (`test_indicator`, `indicator_self_test`) need an X11
   display; on a machine without one, run `ctest` under `xvfb-run -a`.

3. Add or update tests for what you change.
4. Update the documentation when behaviour, build options or the interface
   change. A change to what the program shows goes into the manual in every
   language (`docs/manual/content/<lang>/`); `ctest` checks the pages.
5. Format C++ with clang-format; CI checks it with
   [buildkit's style](https://github.com/medaminben/buildkit).
6. Open a pull request against `Beta` and fill in the template.

## Commits

Conventional Commits: `<type>(optional-scope): <description>` with the types
`feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`,
`chore`, `revert`.

## Code of conduct

Everyone taking part follows the [code of conduct](CODE_OF_CONDUCT.md).
