"""
E2E-tests for ursadb
"""

import subprocess
import os
import sys
from pathlib import Path
import pytest
from typing import Dict, Any, List
import zmq
import shutil
import json


class UrsadbTestContext:
    def __init__(self, ursadb_new: Path, ursadb: Path):
        self.backend = "tcp://127.0.0.1:9876"
        self.tmpdirs = []
        self.ursadb_dir = self.tmpdir()
        ursadb_db = self.ursadb_dir / "db.ursa"

        subprocess.check_call([ursadb_new, ursadb_db])
        self.ursadb = subprocess.Popen([ursadb, ursadb_db, self.backend])

    def __make_socket(self) -> zmq.Context:
        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        socket.setsockopt(zmq.LINGER, 0)
        socket.setsockopt(zmq.RCVTIMEO, 10000)
        socket.connect(self.backend)
        return socket

    def tmpdir(self) -> Path:
        dirpath = os.urandom(8).hex()
        os.mkdir(dirpath)
        self.tmpdirs.append(dirpath)
        return Path(dirpath)

    def request(self, cmd: str) -> Dict[str, Any]:
        sock = self.__make_socket()
        sock.send_string(cmd)
        return json.loads(sock.recv_string())

    def check_request(self, cmd: str) -> Dict[str, Any]:
        response = self.request(cmd)
        assert "error" not in response
        return response

    def close(self):
        for dirpath in self.tmpdirs:
            shutil.rmtree(dirpath)
        self.ursadb.terminate()
        self.ursadb.wait()


@pytest.fixture()
def ursadb(request):
    ursadb_root = Path(__file__).parent.parent / "build"
    ursadb = ursadb_root / "ursadb"
    ursadb_new = ursadb_root / "ursadb_new"

    context = UrsadbTestContext(ursadb_new, ursadb)
    yield context
    context.close()


def store_files(ursadb: UrsadbTestContext, data: Dict[str, bytes]) -> Path:
    tmpdir = ursadb.tmpdir()
    for name, value in data.items():
        (tmpdir / name).write_bytes(value)
    return tmpdir


def test_ping(ursadb: UrsadbTestContext):
    response = ursadb.check_request("ping;")
    assert response["type"] == "ping"
    assert response["result"]["status"] == "ok"


def check_basic_topology(ursadb: UrsadbTestContext):
    """ Given a db context with a single index, does sanity checks """
    response = ursadb.check_request("topology;")
    assert response["type"] == "topology"
    assert len(response["result"]["datasets"]) == 1
    dataset = list(response["result"]["datasets"].values())[0]
    assert dataset["file_count"] == 1
    assert dataset["indexes"][0]["type"] == "gram3"
    assert dataset["indexes"][0]["size"] == dataset["size"]
    assert dataset["taints"] == []

def check_query(ursadb: UrsadbTestContext, query: str, expected: List[str]):
    response = ursadb.check_request(f"select {query};")
    assert response["type"] == "select"
    assert response["result"]["mode"] == "raw"
    assert len(response["result"]["files"]) == len(expected)

    for fpath in response["result"]["files"]:
        assert any(fpath.endswith(f"/{fname}") for fname in expected)


def test_indexing_small(ursadb: UrsadbTestContext):
    dirname = store_files(ursadb, {
        "kot": b"Ala ma kota ale czy kot ma Ale?"
    })
    ursadb.check_request(f'index "{dirname}";')

    check_basic_topology(ursadb)
    check_query(ursadb, '"ale"', ["kot"])
    check_query(ursadb, '":hmm:"', [])

def test_indexing_big(ursadb: UrsadbTestContext):
    dirname = store_files(ursadb, {
        "kot": b"!" * 1024 * 1024 * 20 + b"ale bitmap index builder here!"
    })
    ursadb.check_request(f'index "{dirname}";')

    # TODO: failing, because we create a second, empty dataset. 
    # check_basic_topology(ursadb)
    check_query(ursadb, '"ale"', ["kot"])
    check_query(ursadb, '":hmm:"', [])

def test_gram3_index_works_as_expected(ursadb: UrsadbTestContext):
    dirname = store_files(ursadb, {
        "kot": b"aaaaabb   bbccccc",
        "zzz": b"aaaaabbccccc",
        "yyy": b"\xff\xff\xff",
    })
    ursadb.check_request(f'index "{dirname}" with [gram3];')
    check_query(ursadb, '"abbc"', ["kot", "zzz"])
    check_query(ursadb, '{ff ff ff}', ["yyy"])

def test_text4_index_works_as_expected(ursadb: UrsadbTestContext):
    dirname = store_files(ursadb, {
        "kot": b"aaaaabb   bbccccc",
        "zzz": b"aaaaabbccccc",
        "yyy": b"\xff\xff\xff",
    })
    ursadb.check_request(f'index "{dirname}" with [text4];')
    check_query(ursadb, '"abbc"', ["zzz"])
    check_query(ursadb, '{ff ff ff}', ["kot", "zzz", "yyy"])

def test_wide8_index_works_as_expected(ursadb: UrsadbTestContext):
    dirname = store_files(ursadb, {
        "kot": b"aaaaabb   bbccccc",
        "zzz": b"aaaaabbccccc",
        "yyy": b"\xff\xff\xff",
        "vvv": b"a\x00b\x00c\x00d\x00efgh"
    })
    ursadb.check_request(f'index "{dirname}" with [wide8];')
    check_query(ursadb, '"abbc"', ["kot", "zzz", "yyy", "vvv"])
    check_query(ursadb, '{ff ff ff}', ["kot", "zzz", "yyy", "vvv"])
    check_query(ursadb, '"a\\x00b\\x00c\\x00d\\x00"', ["vvv"])
