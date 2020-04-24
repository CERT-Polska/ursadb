from util import UrsadbTestContext
from util import ursadb  # noqa


def test_ping(ursadb: UrsadbTestContext):
    response = ursadb.check_request("ping;")
    assert response["type"] == "ping"
    assert response["result"]["status"] == "ok"


def test_parse_errors(ursadb: UrsadbTestContext):
    response = ursadb.request("cats are stupid;")
    assert "error" in response


def test_topology(ursadb: UrsadbTestContext):
    ursadb.check_request("topology;", {"datasets": {}})


def test_status(ursadb: UrsadbTestContext):
    ursadb.check_request(
        "status;",
        {
            "tasks": [
                {
                    "connection_id": "#UNK#",
                    "epoch_ms": "#UNK#",
                    "id": "1",
                    "request": "status;",
                    "work_done": 0,
                    "work_estimated": 0,
                }
            ],
            "ursadb_version": "#UNK#",
        },
    )
