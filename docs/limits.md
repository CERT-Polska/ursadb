# limits

There are not many hard limits in ursadb:

 - The number of datasets must be less than 4 billion.
 - The number of files per dataset must be less than 4 billion.
 - Size of every index must be smaller than 18 exabytes.
 - Size of all filenames in the index must be smaller than 18 exabytes.

These are all absurdly large. But there are other factors: RAM, disk space and CPU time.

 - RAM: It's hard to estimate, but with a default configuration and "reasonable" Yara
    rules, a single query will consume at most 8 bytes per file in a dataset.
    But hypothetical very large Yara rules (like OR of 1000 strings) can use more, so
    it's just a rough estimate. Wildcards, when enabled in ursadb, can also consume
    a lot of RAM so should be configured with care (default configuration is safe).
    Large datasets can always be split into smaller ones, so this shouldn't be
    a huge problem.
 - Disk space: See documentation about [index types](./indextypes.md). On our
    collections, combined size of all indexes except `hash4` is about 70% of indexed
    files, and `hash4` index takes another 70% if used.
 - CPU: Of course, querying should always be faster than running Yara directly
    (otherwise, what's the point). But the time of indexing large collections can be
    a problem. It depends a lot on the machine, but using the `utils.index` script
    I was able to index and compact [vx-underground](https://vx-underground.org/)
    malware collection (1.6M files, 600GB) under 10 hours on a reasonably cheap server
    and our production server was able to index 19M files (9TB) over the last weekend.
