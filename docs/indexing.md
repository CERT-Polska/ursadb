# Indexing

First, start ursadb client command prompt

```
ursacli
[2020-05-10 05:23:27.216] [info] Connecting to tcp://localhost:9281
[2020-05-10 05:23:27.219] [info] Connected to UrsaDB v1.3.2+be20951 (connection id: 006B8B45B4)
ursadb>
```

Now, type:

```
ursadb> index "/mnt/samples";
```

To index `"/mnt/samples"` directory. By default this will only use `gram3` index.
It's a good idea to use more indexes for better results:

```
ursadb> index "/mnt/samples" with [gram3, text4, wide8, hash4];
```

There are more variations of this command. For example you can:
 - Index a list of files, or even read that list from file. 
 - Tag all indexed samples with arbitrary metadata.
 - Disable safety measures that protect you from indexing the same file twice.

See [query syntax documentation](./syntax.md#index)

All indexing is part of a single transaction, so when the server crashes indexing
will have to be restarted. This is intentional - because of this it's always possible
to tell which files have been indexed, and the database is in the consistend state.
But it makes indexing really large collections harder.

To avoid this problem, use `utils/index.py` script shipped with
[mquery](https://github.com/CERT-Polska/mquery).
