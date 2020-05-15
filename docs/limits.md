# Limits

There are not many hard limits in ursadb:

 - The number of datasets must be less than 4 billion.
 - The number of files per dataset must be less than 4 billion.
 - Size of every index must be smaller than 18 exabytes.
 - Size of all filenames in the index must be smaller than 18 exabytes.

These are all absurdly large. But there are other factors: RAM, disk space and CPU time.

## Soft limits

**Disk space**

See documentation about [index types](./indextypes.md). In our collections
, combined size of all indexes except `hash4` is about 70% of indexed
files, and `hash4` index takes another 70% if used. For example, 630 GiB of
indexed malware, resulted in the following indexes:

| index type | index size | index / data% |
| ---------- | ---------- | ------------- |
| gram3      | 421 GiB    | 68.23%        |
| text4      | 17 GiB     | 2.75%         |
| wide8      | 4 GiB      | 0.64%         |
| hash4      | 450 GiB    | 72.93%        |

**RAM**

The database won't consume a lot of RAM when doing nothing. But two most
important actions - indexing and querying - can be quite memory intensive.

A generic solution to problems with memory may be lowering `database_workers`
setting, but it's not a perfect solution (when all the workers are busy, the db
won't answer even to simple `status` queries).

All numbers below are a very rough estimates (they don't take small allocations
into account, for example).

**RAM: querying**

It's very hard to estimate, and depends on the
[database configuration](./configuration.md) (especially the `query_*` keys).
If you start getting memory errors for queries and use wildcards, consider
lowering `query_max_edge` and `query_max_ngram` values.

With a default configuration and "reasonable" Yara rules, a single query will
consume at most 8 bytes per file in a dataset. But hypothetical very large Yara
rules (for exmaple, OR of 1000 strings) can use more, so it's just a rough
estimate. Wildcards, when used heavily, can also consume a lot of RAM so
should be configured with care (the default configuration is safe).

Required memory scales linearly with a dataset size, and large datasets can
be split into smaller ones, so this can be easily worked around.

**RAM: compacting**

See documentation for `merge_max_datasets` key in the
[database configuration](./configuration.md#merge_max_datasets).

When merging, datasets must be fully loaded, and every loaded dataset consumes
a bit over 128MiB. In theory you can set `merge_max_datasets` to 2 and live
with only 512MiB of memory, but it'll make compacting morbidly slow. The
healthy setting is 10 though, and there's no real need to increase it further.

**RAM: indexing**

Datasets generated during indexing will be kept in temporary storage and
compacted when necessary (compacting will only happen for large collections).
RAM limits for compacting apply here. Additionaly, indexing consumes up to 512MB
(not confugurable currently) RAM (for example 512MB for `[gram3]`, 1GiB for
`[gram3, text4]`, etc). When saving an index to disk, additional 512MiB of
memory are used temporarily.

Nothing is particularly configurable here. Users should avoid running
too many indexing jobs at once, but a single indexing job is not very heavy.

**CPU and time**

Of course, querying should always be faster than running Yara directly
(otherwise, what's the point). But the time of indexing large collections can be
a problem. It depends a lot on the machine, but using the `utils.index` script
I was able to index and compact [vx-underground](https://vx-underground.org/)
malware collection (1.6M files, 600GB) under 10 hours on a reasonably cheap server
and our production server was able to index 19M files (9TB) over the last weekend.

**Others**

Ursadb keeps one open file handle for every file in the index directory (except
dataset metadata and the main db file). This usually means 2-6 file handles
per dataset, and can become very large for uncompacted databases. Ursadb will
try to increase `RLIMIT_NOFILE` at startup to a large value, so unless you
have additional hardening on your system you don't need to do anything.
