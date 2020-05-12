# on disk format

Ursadb tries to keep its on-disk format simple. There are 7 types of files:

- [`dataset`](#dataset) file (json)
- [`database`](#database) file (json)
- [`index`](#index) file (binary)
- [`names`](#names) file (plaintext)
- [`namecache`](#namecache) file (binary)
- [`itermeta`](#itermeta) file (json)
- [`iterator`](#iterator) file (plaintext)

## database

Contains references to all the other files in ursadb.
If something is not (directly or indirectly) referenced from the database file,
it means that it's not used and can be safely removed (when database is turned off). 

Example:

```
{
    "config": {
        "database_workers": 10
    },
    "datasets": [
        "set.507718ac.db.ursa"
    ],
    "iterators": {
        "01ef4552": "itermeta.01ef4552.db.ursa",
    },
    "version": "1.3.2"
}
```

## dataset

Contains all necesary information about a [dataset](./datasets.md).

Example:
```
{
    "filename_cache": "namecache.files.set.507718ac.db.ursa",
    "files": "files.set.507718ac.db.ursa",
    "indices": [
        "gram3.set.507718ac.db.ursa"
    ],
    "taints": []
}
```

## index

A binary file. Contains a header with this structure:

```cpp
struct OnDiskIndexHeader {
    uint32_t magic;  // magic number (0xCA7DA7A)
    uint32_t version;  // version (fixed to 6, since forever)
    uint32_t raw_type;  // number that represents type of the index
    uint32_t reserved;  // always 0
};
```

Followed by a list of `2**24` compressed sequences of fileids, one for every
trigram.

## names

Newline-separated list of filenames in the database. This file can be safely
edited or changed with any editor, for example when moving collection to a
different folder. It's only important to:
 - ensure the database is turned off
 - remove the namecache file later

## namecache

Contains an array of `uint64_t` values specifying where in the `names` file
the file with a given index starts. This is used to speed up queries.
If this file doesn't exist or was removed, it'll be generated when the database
starts.

## itermeta

Contains information about position of a given iterator. For example:

```
{
    "backing_storage": "iterator.2948dc6b.db.ursa",
    "byte_offset": 722200,
    "file_offset": 15700,
    "total_files": 50135
}
```

## iterator

Newline-separated list of files returned by specified iterator.
`iterator pop` command will read filenames from this file and return them.
