"""Test stability of iterators in a highly multithreaded environment.

Especially, ensure that every file is returned and there are no unexpected
errors.
"""

from .util import UrsadbTestContext, store_files
from .util import ursadb  # noqa
import json
from multiprocessing import Pool
import zmq


def f(params):
    iterator, db_url = params
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.LINGER, 0)
    socket.setsockopt(zmq.RCVTIMEO, 3000)
    while True:
        socket.connect(db_url)
        socket.send_string(f'iterator "{iterator}" pop 1;')
        status = json.loads(socket.recv())
        if "error" in status:
            if status["error"]["retry"]:
                continue
        return status


def concurrent_pop_pass(ursadb: UrsadbTestContext) -> None:
    p = Pool(32)
    res = ursadb.check_request('select into iterator "";')
    iterator = res["result"]["iterator"]
    file_count = res["result"]["file_count"]

    result_files = []
    results = p.map(f, [(iterator, ursadb.backend)] * file_count)
    for result in results:
        if "error" in result:
            print(result["error"])
            assert False
        result_files += result["result"]["files"]

    resp = ursadb.request(f'iterator "{iterator}" pop 1;')
    assert "error" in resp
    assert file_count == len(result_files)
    assert len(set(result_files)) == len(result_files)


def test_concurrent_pop(ursadb: UrsadbTestContext) -> None:
    store_files(
        ursadb, "gram3", {str(i): str(i).encode() for i in range(3000)},
    )

    for i in range(1):
        concurrent_pop_pass(ursadb)
