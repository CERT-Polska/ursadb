# Getting started

**This part of documentation is work in progress and will be improved in the future**

### Installation

The easiest way to start a ursadb instance is to run (substitute your files and index paths):

```
mkdir -p /tmp/ursadb/index /tmp/ursadb/files
sudo docker run -p 9281 -v /tmp/ursadb/index:/var/lib/ursadb:rw -v /tmp/ursadb/files:/mnt/samples certpl/ursadb
```

For other installation methods see [install.md](./install.md).

To connect to the database you can build `ursacli` yourself, or use the tool from docker again:

```
sudo docker ps  # look up container ID
sudo docker exec -it [container ID] ursacli
[2022-08-28 13:38:06.154] [info] Connecting to tcp://localhost:9281
[2022-08-28 13:38:06.155] [info] Connected to UrsaDB v1.3.2+3797f9b (connection id: 006B8B4567)
ursadb>
```

### Indexing

Using another terminal, put some files in the files directory (`/tmp/ursadb/files` in the snippet above).
I'll use the project source code in the example

```
cd /tmp/ursadb/files
cd git clone https://github.com/CERT-Polska/ursadb.git
```

Now send a command to the database

```
ursadb> index "/mnt/samples";
{
    "result": {
        "status": "ok"
    },
    "type": "ok"
}
```

If everything worked correctly, you should have at least one dataset. Check that with a `topology` command:

```
ursadb> topology;
dataset 20d30d28 [       311] (gram3)
```

Finally, query the data for some strings:

```
ursadb> select "BSD";
/mnt/samples/ursadb/extern/catch/Catch.h
/mnt/samples/ursadb/LICENSE
```

That's it. For more available commands see [syntax.md](./syntax.md).