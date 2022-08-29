# Configuration

Ursadb configuration is a simple set of key-value pairs. The defaults are sane.
You don't need to change them, unless you want to tweak ursadb to your system.

They can be read by issuing a `config get` command with `ursacli`:

```
$ ursacli
ursadb> config get;
```

The response format is:

```json
{
    "result": {
        "keys": {
            "database_workers": 10,
            "merge_max_datasets": 10,
            "merge_max_files": 1073741824,
            "query_max_edge": 2,
            "query_max_ngram": 16
        }
    },
    "type": "config"
}
```

To change a config value, you may issue a `config set` command:

```
$ ursacli
ursadb> config set "database_workers" 10;
```

## Available configuration keys

- [database_workers](#database_workers) - Number of independent worker threads.
- [query_max_edge](#query_max_edge) - Maximum query size (edge).
- [query_max_ngram](#query_max_ngram) - Maximum query size (ngram).
- [merge_max_datasets](#merge_max_datasets) - Maximum number of datasets involved
  in a single merge.
- [merge_max_files](#merge_max_files) - Maximum number of datasets in a dataset
  after the merge.

### query_max_edge

- **Default**: 2
- **Minimum**: 1
- **Maximum**: 255

Maximum number of values a first or last character in sequence can take
to be considered when planning a query. The default is a conservative 1,
so query plan will never start or end with a wildcard.

**Recommendation**: Stick to the default value. If you have a good disk and
want to reduce false-positives, increase (but no more than 16).

### query_max_ngram

- **Default**: 16
- **Minimum**: 1
- **Maximum**: 16777215


Maximum number of values a ngram can take to be considered when planning
a query. For example, with a default value of 16, trigram `11 2? 33` will
be expanded and included in query, but `11 ?? 33` will be ignored.

**Recommendation**: Stick to the default value at first. If your queries are
fast, use many wildcards, but have many false positives, increase to 256.


### database_workers

- **Default**: 4
- **Minimum**: 1
- **Maximum**: 1024

How many tasks can be processed at once? The default 4 is a very
conservative value for most workloads. Increasing it will make the
database faster, but at a certain point the disk becomes a bottleneck.
This will also linearly increase memory usage in the worst case.

**Recommendation**: If your server is dedicated to ursadb, or your IO latency
is high (for example, files are stored on NFS), increase to 8 or more.

### merge_max_datasets

- **Default**: 10
- **Minimum**: 1
- **Maximum**: 1024

How many datasets can be merged at once? This has severe memory usage
implications - before merging, datasets must be fully loaded, and every
loaded dataset consumes a bit over 128MiB. Increasing this number makes
compacting huge datasets faster, but may run out of ram.

**Recommendation**: merge_max_datasets * 128MiB can safely be set to around
1/4 of RAM dedicated to the database, so for example 8 for 4GiB server
or 32 for 16GiB server. Increasing past 10 has diminishing returns, so
unless you have a lot of free RAM you can leave it at default.

### merge_max_files

- **Default**: 2097152
- **Minimum**: 1
- **Maximum**: 4294967295

When merging, what is the maximal allowed number of files in the
resulting dataset? Large datasets make the database faster, but also need
more memory to run efficiently.

**Recommendation**: ursadb was used with multi-million datasets in the wild,
but currently we recommend to keep
datasets smaller than 1 million files.
