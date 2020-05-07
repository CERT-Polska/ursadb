"""
E2E-tests for ursadb
"""

import subprocess
import os
import resource
from pathlib import Path
import pytest
from typing import Dict, Any, List
import zmq
import hashlib
import shutil
import json
import tempfile


class UrsadbConfig:
    def __init__(
        self,
        rlimit_ram: int = None,
        merge_max_datasets: int = None,
        merge_max_files: int = None,
        query_max_ngram: int = None,
        query_max_edge: int = None,
    ) -> None:
        self.rlimit_ram = rlimit_ram
        self.raw_config = {
            "merge_max_datasets": merge_max_datasets,
            "merge_max_files": merge_max_files,
            "query_max_edge": query_max_edge,
            "query_max_ngram": query_max_ngram,
        }


class UrsadbTestContext:
    def __init__(self, ursadb_new: Path, ursadb: Path, config: UrsadbConfig):
        self.backend = "tcp://127.0.0.1:9876"
        self.tmpdirs = []
        self.ursadb_dir = self.tmpdir()
        self.db = self.ursadb_dir / "db.ursa"

        def configure():
            # Set maximum CPU time to 1 second in child process, after fork() but before exec()
            if config.rlimit_ram is not None:
                resource.setrlimit(
                    resource.RLIMIT_AS, (config.rlimit_ram, config.rlimit_ram)
                )

        subprocess.check_call([ursadb_new, self.db])
        self.ursadb = subprocess.Popen(
            [ursadb, self.db, self.backend], preexec_fn=configure
        )

        for key, value in config.raw_config.items():
            if value is None:
                continue
            self.check_request(f'config set "{key}" {value};')

    def __make_socket(self) -> zmq.Context:
        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        socket.setsockopt(zmq.LINGER, 0)
        socket.setsockopt(zmq.RCVTIMEO, 60000)
        socket.connect(self.backend)
        return socket

    def tmpdir(self) -> Path:
        dirpath = tempfile.gettempdir() + "/" + os.urandom(8).hex()
        os.mkdir(dirpath)
        self.tmpdirs.append(dirpath)
        return Path(dirpath)

    def request(self, cmd: str) -> Dict[str, Any]:
        sock = self.__make_socket()
        sock.send_string(cmd)
        return json.loads(sock.recv_string())

    def start_request(self, cmd: str) -> zmq.Context:
        sock = self.__make_socket()
        sock.send_string(cmd)
        return sock

    def check_request(self, cmd: str, pattern: Any = None) -> Dict[str, Any]:
        response = self.request(cmd)
        if "error" in response:
            print(json.dumps(response))
            assert False
        if pattern:
            matches = match_pattern(response["result"], pattern)
            if not matches:
                # for better error message
                print(json.dumps(pattern, indent=4))
                print(json.dumps(response["result"], indent=4))
                assert matches
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

    config = UrsadbConfig()
    if hasattr(request, "param"):
        config = request.param
    context = UrsadbTestContext(ursadb_new, ursadb, config)
    yield context
    context.close()


def match_pattern(value: Any, pattern: Any):
    if isinstance(pattern, dict):
        # If the pattern is a dict, every key must match to value.
        if not isinstance(value, dict):
            return False
        if len(value.keys()) != len(pattern.keys()):
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
    ursadb: UrsadbTestContext,
    type: str,
    data: Dict[str, bytes],
    expect_error: bool = False,
) -> None:
    """ Stores files on disk, and index them. Make sure to be
    deterministic, because we're using this for tests. """
    tmpdir = ursadb.tmpdir()
    filenames = []
    for name, value in sorted(data.items()):
        (tmpdir / name).write_bytes(value)
        filenames.append(str(tmpdir / name))

    ursa_names = " ".join(f'"{f}"' for f in filenames)

    if expect_error:
        res = ursadb.request(f"index {ursa_names} with [{type}];")
        assert "error" in res
    else:
        ursadb.check_request(f"index {ursa_names} with [{type}];")


def check_query(ursadb: UrsadbTestContext, query: str, expected: List[str]):
    response = ursadb.check_request(f"select {query};")
    assert response["type"] == "select"
    assert response["result"]["mode"] == "raw"
    assert len(response["result"]["files"]) == len(expected)

    for fpath in response["result"]["files"]:
        assert any(fpath.endswith(f"/{fname}") for fname in expected)


def get_index_hash(ursadb: UrsadbTestContext, type: str) -> str:
    """ Tries to find sha256 hash of the provided index """
    indexes = list(ursadb.ursadb_dir.glob(f"{type}*"))
    assert len(indexes) == 1
    return hashlib.sha256(indexes[0].read_bytes()).hexdigest()
