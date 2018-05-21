UrsaDB2
=======

A fast N-gram database optimized for working with executable files (malware dumps).
Rewritten from Rust. Originally by Jarosław Jedynak (tailcall.net).


Installation (Docker way)
-------------------------

Docker image may be built by executing `docker build -t ursadb .` on the source code pulled from this repo.


Installation (standard way)
---------------------------

1. Compile from sources:
```
$ mkdir build
$ cd build
$ cmake -D CMAKE_C_COMPILER=gcc-7 -D CMAKE_CXX_COMPILER=g++-7 -D CMAKE_BUILD_TYPE=Release ..
$ make
```

2. Deploy output binaries (`ursadb`, `ursadb_new`) to appropriate place, e.g:
```
# cp ursadb ursadb_new /usr/local/bin/
```

3. Create new database:
```
$ mkdir /opt/ursadb
$ ursadb_new /opt/ursadb/db.ursa
```

4. Run UrsaDB server:
```
$ ursadb /opt/ursadb/db.ursa
```

5. (Optional) Consider registering UrsaDB as a systemd service:
```
cp contrib/systemd/ursadb.service /
systemctl enable ursadb
```


Usage
-----

Interaction with the database could be done using `ursadb2-client` (see another repository).


Queries
-------

### Indexing
A filesystem path could be indexed using `index` command:

```
index "/opt/something";
```

by default it will be indexed using `gram3` index. Index types may be specified manually:

```
index "/opt/something" with [gram3, text4, hash4, wide8];
```

### Select
Select queries could use ordinary strings, hex strings and wide strings.

Query for ASCII bytes `abc`:
```
select "abc";
```

The same query with hex string notation:
```
select {616263};
```

Query for wide string `abc` (the same as `{610062006300}`):
```
select w"abc";
```

Elements could be AND-ed:
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

### Status
Query for status of tasks running in the database:
```
status;
```

Output format is `\t` separated table where columns mean:
```
task_id    work_done    work_estimated    conn_id    command_str
```

### Topology
Check current database topology - what datasets are loaded and which index types they use.
```
topology;
```

Exemplary output:
```
> topology;
OK
DATASET aa266884
INDEX aa266884 gram3
INDEX aa266884 text4

DATASET bc43a921
INDEX bc43a921 gram3
INDEX bc43a921 text4
```

Means that there are two datasets (partitions), both backed with indexes of type `gram3` and `text4`.

### Reindex
Add new index type to the existing dataset. Before reindexing, you need to determine the ID of dataset
which has to be indexed (may be done using `topology` command).

Example:
```
reindex "bc43a921" with [hash4];
```

will reindex already existing dataset `bc43a921` with `hash4` type index.

### Compact
Force database compacting.

In order to force compacting of all datasets into single one:
```
compact all;
```

In order to force smart compact (database will decide which datasets do need compacting, if any):
```
compact smart;
```
