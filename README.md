# UrsaDB

A 3gram search engine for querying terabytes of data in milliseconds. Optimized for working with binary files (for example, malware dumps).

Created in [CERT.PL](https://cert.pl). Originally by Jarosław Jedynak ([tailcall.net](https://tailcall.net)), extended and improved by Michał Leszczyński.

**This repository is only for UrsaDB project (ngram database). See [CERT-Polska/mquery](https://github.com/CERT-Polska/mquery) for more user friendly UI.**

## Installation

See [installation instructions](./INSTALL.md)

## Quickstart

1. Create new database:
```
mkdir /opt/ursadb
ursadb_new /opt/ursadb/db.ursa
```

2. Run UrsaDB server:
```
ursadb /opt/ursadb/db.ursa
```

3. Connect with UrsaCLI:
```
$ ursacli
[2020-04-13 18:16:36.511] [info] Connected to UrsaDB v1.3.0 (connection id: 006B8B4571)
ursadb>
```

4. [Index some files](./docs/indexing.md):
```
ursadb> index "/opt/samples" with [gram3, text4, wide8, hash4];
```

5. Now you can perform queries. For example, match all files with three null bytes:
```
ursadb> select {00 00 00};
```

Read the [syntax](./docs/syntax.md) documentation to learn more about available commands.

## Learn more

More documentation can be found in the [docs](./docs/) directory.

You can also read the hosted version here:
[cert-polska.github.io/ursadb](https://cert-polska.github.io/ursadb).

## Contact
If you have any problems, bugs or feature requests related to UrsaDB, you're encouraged to create a GitHub issue.

## Funding acknowledgement
![Co-financed by the Connecting Europe Facility by of the European Union](https://www.cert.pl/wp-content/uploads/2019/02/en_horizontal_cef_logo-1.png)
