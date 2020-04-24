"""
E2E-tests for ursadb
"""

import subprocess
import os
from pathlib import Path
import pytest
from typing import Dict, Any
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
        socket.setsockopt(zmq.RCVTIMEO, 30000)
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

    def check_request(self, cmd: str, pattern: Any = None) -> Dict[str, Any]:
        response = self.request(cmd)
        assert "error" not in response
        if pattern:
            assert match_pattern(response["result"], pattern)
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


def match_pattern(value: Any, pattern: Any):
    if isinstance(pattern, dict):
        # If the pattern is a dict, every key must match to value.
        if not isinstance(value, dict):
            return False
        for k, v in pattern.items():
            # Special #UNK# keys match to any key
            if k.startswith("#UNK#"):
                if not match_pattern(list(value.values()), [v]):
                    return False
            elif k not in value or not match_pattern(value[k], v):
                return False
        return True
    if isinstance(pattern, list):
        # If the pattern is a list, value must contain matching element
        # for every element in the pattern.
        if not isinstance(value, list):
            return False
        if len(value) < len(pattern):
            return False
        for p in pattern:
            for v in value:
                if match_pattern(v, p):
                    break
            else:
                return False
        return True
    return pattern == "#UNK#" or value == pattern


def store_files(
    ursadb: UrsadbTestContext, type: str, data: Dict[str, bytes]
) -> None:
    """ Stores files on disk, and index them. Make sure to be
    deterministic, because we're using this for tests. """
    tmpdir = ursadb.tmpdir()
    filenames = []
    for name, value in sorted(data.items()):
        (tmpdir / name).write_bytes(value)
        filenames.append(str(tmpdir / name))

    ursa_names = " ".join(f'"{f}"' for f in filenames)

    ursadb.check_request(f'index {ursa_names} with [{type}];')
