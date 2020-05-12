# on-disk format

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
it means that it's not used and can be safely removed (when the database is turned off). 

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

After the header, there is a list of `2**24` compressed sequences of file IDs,
with no additional metadata. Sequences are stored differentially and
encoded with [VLQ universal codes](https://en.wikipedia.org/wiki/Variable-length_quantity).
For example:

```
1, 2, 3, 5, 7, 15, 200, 250

# compute differences and subtract 1)
# (2-1-1 = 1) (3-2-1 = 0) (5-3-1 = 1) (7-5-1 = 1) (15-7-1 = 1) (200-15-1 = 184)...

1, 0, 1, 1, 1, 184, 49

# encode integers in base128 using highest bit as continuation bit.
# 184 = 10111000 -> [1][0111000] -> [10111000][1] -> 184, 1

1, 0, 1, 1, 1, 184, 1, 49
```

Finally, the last `(2**24 + 1) * 8` bytes of an index consists of an array of uint64_t
values, where sequence N starts at `array[N]` offset in the file, and ends at `array[N+1]`

An index can be parsed with the following Python code. Warning this is just a demonstration,
and is way too slow to work with indexes bigger than really small ones.

```python
import sys
import struct


def chunks(data, n):
    return [data[i*n:(i+1)*n] for i in range(len(data)//n)]


def decompress(raw: bytes):
    fids = []
    acc = 0
    shift = 0
    prev = -1
    for b in raw:
        acc += (b & 0x7F) << shift
        shift += 7
        if ((b & 0x80) == 0):
            prev += acc + 1
            fids += [prev]
            acc = 0
            shift = 0
    return fids


def parse(fpath):
    with open(fpath, "rb") as f:
        fdata = f.read()

    offsetraw = fdata[-(2**24+1)*8:]
    offsets = struct.unpack('<' + 'Q'*(2**24+1), offsetraw)

    for i in range(2**24):
        run_size = offsets[i+1] - offsets[i]
        if run_size == 0:
            continue

        run = decompress(fdata[offsets[i]:offsets[i+1]])
        print(f"{i:06x}: {run}")


if __name__ == '__main__':
    parse(sys.argv[1])
```

## names

Newline-separated list of filenames in the database. This file can be safely
edited or changed with any editor, for example when moving the collection to a
different folder. It's only important to:

 - ensure the database is turned off
 - remove the namecache file later

## namecache

Contains an array of `uint64_t` offsets in the `names` file.
This is used to map file IDs to names for queries, without loading all the file
names into memory.
If this file doesn't exist or was removed, it'll be regenerated when the database
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

Newline-separated list of files returned by the specified iterator.
`iterator pop` command will read filenames from this file and return them.
