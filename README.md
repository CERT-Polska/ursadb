# UrsaDB

A 3gram search engine for querying Terabytes of data in milliseconds. Optimized for working with binary files (for example, malware dumps).

Created in [CERT.PL](https://cert.pl). Originally by Jarosław Jedynak ([tailcall.net](https://tailcall.net)), extended and improved by Michał Leszczyński.

**This repository is only for UrsaDB project (ngram database). See [CERT-Polska/mquery](https://github.com/CERT-Polska/mquery) for more user friendly UI.**

## Installation

See [installation instructions](./INSTALL.md)

## Quick start

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

4. Index some files:
```
ursadb> index "/opt/samples";
```

5. Now you can perform queries (e.g. match all files with three null bytes):
```
ursadb> select {00 00 00};
```

Check out the [syntax](./docs/syntax.md) section in documentation to learn more
about available commands.

## Contact
If you have any problems, bugs or feature requests related to UrsaDB, you're encouraged to create a GitHub issue. If you have other questions or want to contact the developers directly, you can email:

* Jarosław Jedynak (msm@cert.pl)
* Michał Leszczyński (monk@cert.pl)
* CERT.PL (info@cert.pl)

## Founding acknowledgement
![Co-financed by the Connecting Europe Facility by of the European Union](https://www.cert.pl/wp-content/uploads/2019/02/en_horizontal_cef_logo-1.png)
