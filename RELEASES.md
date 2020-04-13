# Version 1.3.1

### New features

#### Native client

Built-in command line client `ursacli`.

- Interactive mode/single command mode
- Task progress is reported every second
- Very basic output processing

# Version 1.3.0

### New features

#### Taints

Dataset tainting. Datasets with incompatible taints will never get merged.

- New command: `dataset "xxx" taint "yyy"` - adds taint `yyy` to `xxx`.
- New command: `dataset "xxx" untaint "yyy"` - removes taint `yyy` from `xxx`.
- New construct: `select with taints ["xxx"] "stuff"` - select that only
    looks in datasets with specified tags.

#### Iterators

Huge memory saver for large results. Store query results on disk, instead
of dumping them with json to the client.

- New command `select into iterator "stuff"` - select that returns iterator.
- New command `iterator "itername" pop 123` - pops element from the specified
    iterator.

### Performance

- Enable IPO for release builds.

### Internals

- New lock modes - new coordinator command to lock iterator.
- Rewrite logging to spdlog, instead of using std::cout like savages.

### Code quality, tests and CI

- New target `format` added to CMakeLists. `make format` will reformat the
    whole code. We also reformatted our code to match new style.
- DatabaseName introduced, code expected to be slowly rewritten to use it.
- We now run clang-format after every commit in our CI pipeline.
- Run `make build` and unit tests after every build in the CI.
- Build the docker image automatically in our CI pipeline.
- Create a automatic pipeline for building deb packages.
- Refactor cmake lists, and revamp the project sructure.
- Use std::thread instead of raw pthread in the network service code.
- Add missing unit tests for parsing new commands (and some forgotten ones).
