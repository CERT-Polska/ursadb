# syntax

Available commands:

- [`index`](#index)
- [`select`](#select)
- [`select into`](#select-into)
- [`status`](#status)
- [`compact`](#compact)
- [`topology`](#topology)
- [`reindex`](#reindex)
- [`iterator pop`](#iterator-pop)
- [`dataset taint`](#dataset-taint)
- [`dataset untaint`](#dataset-untaint)
- [`dataset drop`](#dataset-drop)
- [`config get`](#config-get)
- [`config set`](#config-set)

All responses from the database use the JSON format.

## index

Examples:

- `index "/opt/something";`
- `index "/opt/something" "/opt/foobar";`
- `index "/opt/something" with [gram3, text4, hash4, wide8];`
- `index from list "/tmp/file-list.txt";`
- `index "/opt" with taints ["kot"];`
- `index "/opt" nocheck;`

### basic indexing

A directory can be indexed recursively with `index` command:

```
index "/opt/something";
```

Multiple paths can be specified at once:

```
index "/opt/something" "/opt/foobar";
```

By default, `gram3` index will be used (this is probably [not what you want](./indextypes.md). Index types may be specified manually:

```
index "/opt/something" with [gram3, text4, hash4, wide8];
```

### read a list of files from file

It's also possible to read a file containing a list of targets to be indexed,
each one separated by a newline.

```
index from list "/tmp/file-list.txt"
```

or

```
index from list "/tmp/file-list.txt" with [gram3, text4, hash4, wide8];
```

`/tmp/file-list.txt` may contain for example:

```
/opt/something
/opt/foobar
```

### Tag files when indexing

It's possible to tag immediately files during indexing, with a special syntax:

```
index "/opt" with taints ["kot"]
```

### Disable safety measures

By default, every new file will be cross-checked with the current state of
the database, to ensure that no duplicates are added. Users can opt-out
of this (for performance or other reasons) with `nocheck` modifier.

```
index "/opt" nocheck
```

Of course, care should be taken in this case.

### Response format

```
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```

## Select

Examples:

- `select "abc";`
- `select {616263};`
- `select w"abc";`
- `select "abc" & "bcd";`
- `select "abc" | "bcd";`
- `select min 2 of ("abcd", "bcdf", "cdef");`
- `select "a\x??c";`
- `select {61 ?? 63};`
- `select {61 3? 63};`
- `select { (61 | 62 | 63) };`
- `select with taints ["tag_name"] {11 22 33};`
- `select with datasets ["tag_name"] {11 22 33};`
- `select into iterator {11 22 33};`


### Strings ("primitives")

Select queries can specify ordinary strings, hex strings and wide strings.

To find files with ASCII bytes `abc`:
```
select "abc";
```

The same query with hex string notation:
```
select {616263};
```

Query for wide string `abc` (equivalent to `{610062006300}`):
```
select w"abc";
```

### Logical operators

Elements can be AND-ed:
```
select "abc" & "bcd";
```

and OR-ed:

```
select "abc" | "bcd";
```

Queries may also use parenthesis:

```
select ("abc" | "bcd") & "cde";
```

### Minimum operator

You may want to look for samples that contain at least N of M strings:

```
select min 2 of ("abcd", "bcdf", "cdef");
```

is equivalent to:

```
select ("abcd" & "bcdf") | ("abcd" & "cdef") | ("bcdf" & "cdef");
```

Note that `min N of (...)` is executed more efficiently than the latter "combinatorial" example. Such syntax corresponds directly to Yara's
"[sets of strings](https://yara.readthedocs.io/en/v3.4.0/writingrules.html#sets-of-strings)" feature.

This operator accepts arbitrary expressions as it's arguments, e.g.:
```
select min 2 of ("abcd" & "bcdf", "lorem" & "ipsum", "hello" & "hi there");
```
in this case inner expressions like `"abcd" & "bcdf"` will be evaluated first.

Minimum operator could be also nested in some expression, e.g.:
```
select "abcd" | ("cdef" & min 2 of ("hello", "hi there", "good morning"));
```

### Wildcards

You can use wildcards in your queries. For example:

```
select "a\x??c";
# or
select {61 ?? 63};
```

Wildcards can be partial:

```
select "a\x3?c";
# or
select {61 3? 63};
```

You can also use alternatives (currently limited to a single byte):

```
select { (61 | 62 | 63) };
```

### Filtering results

You might want to only select files in datasets with a certain tag:

```
select with taints ["tag_name"] {11 22 33};
```

You can also select files in a certain dataset:

```
select with datasets ["dataset_id"] {11 22 33};
```

### Response format

```
{
    "result": {
        "files": [
            "/mnt/samples/10b109fdba6667f3f47eeb669fd7d441",
            "/mnt/samples/a5a4184e8ed34d828ed8bf1279152c31"
        ],
        "mode": "raw"
    },
    "type": "select"
}
```

## Select into

When a select result can be large, it's a good idea to save it to a temporary
iterator:

```
select into iterator {11 22 33};
```

Files can be read later incrementally with `iterator pop` command.

### Response format

```
{
    "result": {
        "file_count": 50402,
        "iterator": "6733522e",
        "mode": "iterator"
    },

    "type": "select"
}
```

## Iterator pop

Use to read iterator created with `iterator pop` command:

```
iterator "iterator_id" pop 3;
```

### Response format

```
{
    "result": {
        "files": [
            "/mnt/samples/malware.exe",
            "/mnt/samples/exception1",
            "/mnt/samples/exception2"
        ],
        "iterator_position": 3,
        "mode": "raw",
        "total_files": 50401
    },
    "type": "select"
}
```

## Status

To check the status of tasks running in the database:
```
status;
```

### Response format

```
{
    "result": {
        "tasks": [
            {
                "connection_id": "006B8B4576",
                "epoch_ms": 40351516,
                "id": 18,
                "request": "compact all;",
                "work_done": 12111420,
                "work_estimated": 16777216
            }
        ],
        "ursadb_version": "1.3.2+88ccbff"
    },
    "type": "status"
}
```

## Topology
Check current database topology - what datasets are loaded and which index types they use.
```
topology;
```

### Response format

```
{
    "result": {
        "datasets": {
            "507718ac": {
                "file_count": 50401,
                "indexes": [
                    {
                        "size": 3117031264,
                        "type": "gram3"
                    }
                ],
                "size": 3117031264,
                "taints": []
            }
        }
    },
    "type": "topology"
}
```

## Reindex

Add new index type to an existing dataset.

For example:
```
reindex "bc43a921" with [gram3, hash4];
```

will change the type of existing dataset `bc43a921` to [`gram3`, `hash4`].

### Response format

```
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```


## Compact
Force database compacting.

To force compacting of all datasets into a single one:
```
compact all;
```

You can also let the database decide if it needs compacting or not
(recommended option):
```
compact smart;
```

These commands will never create a dataset with more than `merge_max_files` files,
and will never compact more than `merge_max_datasets` at once.
To ensure that the database is in a true minimal state, you may need to run
the compact command multiple times.

### Response format

```
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```

## Dataset taint

Add a tag to a dataset.

```
dataset "dataset_id" taint "tag_name";
```

### Response format

```
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```

## Dataset untaint

Remove a tag from a dataset.

```
dataset "dataset_id" untaint "tag_name";
```

### Response format

```
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```

## Dataset drop

Remove a dataset from the database.

```
dataset "dataset_id" drop "tag_name";
```

### Response format

```
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```

## Config set

Change a configuration variable

```
config set "merge_max_files" 10000000;
```

### Response format

```
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```

## Config gets

Get configuration variables:

```
config get "merge_max_files"
```

You can also ask for multiple values at once:

```
config get "merge_max_files" "merge_max_datasets"
```

Or even for all values:

```
config get
```

### Response format

```
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
