from .util import UrsadbTestContext
from .util import ursadb  # noqa


def test_config_get_default(ursadb: UrsadbTestContext) -> None:
    """ Test config get command for a valid key """
    ursadb.check_request(
        'config get "query_max_ngram";', {"keys": {"query_max_ngram": 16}},
    )


def test_config_get_many(ursadb: UrsadbTestContext) -> None:
    """ Test config get command for multiple keys """
    ursadb.check_request(
        'config get "query_max_ngram" "query_max_edge";',
        {"keys": {"query_max_ngram": 16, "query_max_edge": 2}},
    )


def test_config_get_all(ursadb: UrsadbTestContext) -> None:
    """ Test config get command for all keys """
    response = ursadb.request("config get;")
    assert "result" in response
    assert "keys" in response["result"]


def test_config_get_error(ursadb: UrsadbTestContext) -> None:
    """ Test config get command for invalid key """
    resp = ursadb.request('config get "invalid"')
    assert "error" in resp


def test_config_set(ursadb: UrsadbTestContext) -> None:
    """ Test config change command """
    ursadb.check_request('config set "query_max_ngram" 1337;')

    ursadb.check_request(
        'config get "query_max_ngram";', {"keys": {"query_max_ngram": 1337}},
    )


def test_config_set_min(ursadb: UrsadbTestContext) -> None:
    """ Sanity check that minimal values are enforced """
    assert "error" in ursadb.request('config set "query_max_edge" 0;')


def test_config_set_max(ursadb: UrsadbTestContext) -> None:
    """ Sanity check that maximum values are enforced """
    assert "error" in ursadb.request('config set "query_max_edge" 256;')
