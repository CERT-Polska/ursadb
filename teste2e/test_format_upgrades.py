from .util import UrsadbTestContext
from .util import ursadb  # noqa
import json
import subprocess
from pathlib import Path
import tempfile
import os
import zmq
import json
from typing import Dict, Any, Optional


class TemporaryStorage:
    def __init__(self):
        self.tmpfiles = []
        self.tmpdir = tempfile.gettempdir()

    def tmpfile(self, name: Optional[str] = None) -> Path:
        if name is None:
            filepath = self.tmpdir + "/" + os.urandom(8).hex()
        else:
            filepath = self.tmpdir + "/" + name
        self.tmpfiles.append(filepath)
        return Path(filepath)

    def write_json(
        self, data: Dict[str, Any], name: Optional[str] = None
    ) -> Path:
        p = self.tmpfile(name=name)
        p.write_text(json.dumps(data))
        return p

    def write_text(self, text: str, name: Optional[str] = None) -> Path:
        p = self.tmpfile(name=name)
        p.write_text(text)
        return p

    def free(self):
        for fpath in self.tmpfiles:
            os.unlink(fpath)


def upgrade_json(data: Dict[str, Any]) -> Dict[str, Any]:
    """ Tries to upgrade database passed in Dict. Will create
    a whole tree of database objects, in order to pass db checks. """
    ursadb_root = Path(__file__).parent.parent / "build"
    ursadb = ursadb_root / "ursadb"
    backend = "tcp://127.0.0.1:9877"

    ctx = TemporaryStorage()

    db = ctx.write_json(data)
    for dataset in data.get("datasets", []):
        filesf = ctx.write_text("").name
        ctx.write_json(
            {"files": filesf, "indices": [], "taints": []}, name=dataset
        )

    if data.get("iterators") is not None:
        for itermeta in data["iterators"].values():
            iterf = ctx.write_text("").name
            ctx.write_json(
                {
                    "backing_storage": iterf,
                    "byte_offset": 0,
                    "file_offset": 0,
                    "total_files": 0,
                },
                name=itermeta,
            )

    ursadb = subprocess.Popen([ursadb, db, backend])

    # Wait for the database to boot.
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.LINGER, 0)
    socket.setsockopt(zmq.RCVTIMEO, 10000)
    socket.connect(backend)
    socket.send_string("status;")
    socket.recv_string()

    ursadb.terminate()

    return json.loads(db.read_text())


def test_new_database(ursadb: UrsadbTestContext):
    raw_data = json.loads(ursadb.db.read_text())

    assert raw_data == {
        "config": {},
        "datasets": [],
        "iterators": {},
        "version": "1.3.2",
    }


def test_upgrade_from_v1_0_0_clean():
    upgrade = upgrade_json(
        {
            "config": {"max_mem_size": 2147483648},
            "datasets": [],
            "iterators": None,
        }
    )
    assert upgrade == {
        "config": {},
        "datasets": [],
        "iterators": {},
        "version": "1.3.2",
    }


def test_upgrade_from_v1_0_0_dirty():
    upgrade = upgrade_json(
        {
            "config": {"max_mem_size": 2147483648},
            "datasets": ["set.72fa9b58.db.ursa", "set.964c6279.db.ursa"],
            "iterators": {"00c16214": "itermeta.00c16214.db.ursa"},
        }
    )
    assert upgrade == {
        "config": {},
        "datasets": ["set.72fa9b58.db.ursa", "set.964c6279.db.ursa"],
        "iterators": {"00c16214": "itermeta.00c16214.db.ursa"},
        "version": "1.3.2",
    }
