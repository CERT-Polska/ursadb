"""Test stability of compacting in a highly multithreaded environment.

Especially, ensure that merge don't lose files, and there can't be two merge
operations at once.
"""

from .util import UrsadbTestContext, store_files
from .util import ursadb  # noqa
from multiprocessing import Pool
import zmq
import time
import json


def f(params):
    ndx, db_url = params
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.LINGER, 0)
    socket.setsockopt(zmq.RCVTIMEO, 30000)
    socket.connect(db_url)

    if ndx % 2 == 0:
        # check status
        socket.send_string("status;")
        r = socket.recv_string()
        r = json.loads(r)
        if "error" in r:
            return r
        for task in r["result"]["tasks"]:
            if task["work_estimated"] != 0:
                return {"error": task}
            if task["work_done"] != 0:
                return {"error": task}
            if task["id"] > 4000:
                return {"error": task}
            if (
                "status" not in task["request"]
                and "topology" not in task["request"]
            ):
                return {"error": task}
        return r
    else:
        # do some other work
        socket.send_string("topology;")
        return socket.recv_string()


def test_status_results_are_stable(ursadb: UrsadbTestContext) -> None:
    """ Test what happens when we index multiple times at once """
    store_files(
        ursadb, "gram3", {"file1name": b"file1data"},
    )
    store_files(
        ursadb, "gram3", {"file2name": b"file2data"},
    )

    p = Pool(32)
    for r in p.map(f, [(i, ursadb.backend) for i in range(1000)]):
        if "error" in r:
            print(r)
            assert False
