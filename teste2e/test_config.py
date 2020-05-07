from .util import UrsadbTestContext
from .util import ursadb  # noqa


def test_config_get_default(ursadb: UrsadbTestContext):
    ursadb.check_request(
        'config get "query_max_ngram";', {"keys": {"query_max_ngram": 16}},
    )


def test_config_get_many(ursadb: UrsadbTestContext):
    ursadb.check_request(
        'config get "query_max_ngram" "query_max_edge";',
        {"keys": {"query_max_ngram": 16, "query_max_edge": 2}},
    )


def test_config_get_all(ursadb: UrsadbTestContext):
    response = ursadb.request("config get;")
    assert "result" in response
    assert "keys" in response["result"]


def test_config_get_error(ursadb: UrsadbTestContext):
    resp = ursadb.request('config get "invalid"')
    assert "error" in resp


def test_config_set(ursadb: UrsadbTestContext):
    ursadb.check_request('config set "query_max_ngram" 1337;')

    ursadb.check_request(
        'config get "query_max_ngram";', {"keys": {"query_max_ngram": 1337}},
    )
