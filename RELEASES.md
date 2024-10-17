# Version 1.5.2 (2024-10-17)

Almost exclusively performance improvements. Query speed is reduced by over 50% in tested workloads.

Features:
- Add a verbose mode (-v) that also prints debug logs (#218) 

Performance:
- Refactor  (#218) 
- Add debug logs, including query plans (#218)
- opt1: Flatten trivial operations (#220)
- opt2: Inline suboperations (#221)
- opt3: Deduplicate primitives (#222)
- opt4: Simplify minof in some cases (#223)
- opt5: Propagate degenerate queries (#224)
- opt6: Reorder subqueries more optimally (#225)
- opt7: Prefetch disk reads (#226)
- opt8: Keep runs in compressed form for longer (#227)

Correctness:

Refactoring and maintenance:
- Create a simple framework for query optimizations (#217) 

# Version 1.5.1 (2023-01-04)

Mostly bugfix and maintenance release:

Features:
- Implement GC for iterators - remove stale iterators (#208)

Performance:
- Move away from query graphs to query plans (#191)
- And improve their performance (#194)

Correctness:
- Fix Undefined Behaviour when getting a memory map size (#188)
- Add support for 2gb+ iterators (fix signed i32 overflow) (#202)

Refactoring and maintenance:
- Bump catch v2.2.2 -> v2.13.10 (#192) 
- Disable clang-tidy, which has become very noisy (#193)
- Add performabce counters for unique ngram reads (#199)
- Remove dead code accrued over the years (#200)

# Version 1.5.0 (2022-08-29)

Features:
- Alternatives (like {(41 | 42)}) implemented in the ursadb query syntax (#65)
- Better support for wildcards (#23)
- Syntax for indexing with taints (#31)

Performance:
- Query graph pruning (#67)

Correctness:
- Some improvements for thread safety (#32)

Refactoring and maintenance:
- Ursacli rewritten to C++ (#48)
- Documentation improvements (#33)

# Version 1.4.2 (2020-06-10)

Bugfix release

### Bugfixes

- Handle `min ... of` correctly when reducing to `and` (#157).

# Version 1.4.1

Bugfix release

### Bugfixes

- Handle `index from list` command properly when the target is a directory
- Improve performance for `wide ascii` queries (more generally, for alternatives).

# Version 1.4.0

One of the bigger releases since the initial version.

### New features

#### QueryGraphs

A big rework of how queries work under the hood. Doesn't change anything at
the first sight, but they make a lot of more comples optimisations possible.
Thanks to this, wildcard queries finally become practical.
[Marvel at their beauty](https://github.com/CERT-Polska/ursadb/blob/71516482bca89d288299fc9b74fa13f04fb53282/libursa/QueryGraph.h)

#### Syntax extensions and new commands

- `nocheck` modifier for index.
- `select with datasets` filter for select.
- `index with taints` index and then tag immediately.
- Alternatives (`(11 | 22 | 33)`) syntax added for selects.
- `select` now returns performance counters along with results (this is currently
    intentionally undocumented, and may be subject to change).
- `drop dataset` command.
- `config get` and `config set` commands - together with four configuration
    variables that make the database configurable.

#### Ursacli improvements

- Special treatment of `status` and `topology` commands.

### Performance

- Namecache files are not removed and are referenced in datasets now - so they
    don't have to be regenerated every time, which makes database (re)start faster.
- Number of database workers is now tuneable with configuration (`database_workers`)

### Bugfixes

- Bump rlimit when starting mquery to a much higher value. By default linux tries
    to constrain our database to meagre 1000 files, can you believe that?

### Code quality, tests and CI

- Coverity added to tools used to scan the code.
- A lot of tests added - including end2end tests that were previously missing.
- [Documentation](./docs/) added and extended.

# Version 1.3.2

### Bugfixes

* Improved stability of `ursacli`.

### New features

#### Native client

In `ursacli` console client:

- Introduced `-c` (command), `-q` (non-interactive mode) and `-j` (always output raw JSON) command line switches.
- Better result printing for `select` query results.

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
