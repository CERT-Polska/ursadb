import unittest
import zmq  # type: ignore
import json
import os
import logging


class TestUrsaQuery(unittest.TestCase):
    def test_ursa_query(self):
        logging.info("Create files")
        with open("/mnt/samples/file1.txt", "w") as f:
            f.write("hello")
        with open("/mnt/samples/file2.txt", "w") as f:
            f.write("hello")

        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        socket.connect("tcp://ursadb:9281")

        socket.send_string(
            'index "/mnt/samples" with [gram3, text4, hash4, wide8];'
        )
        assert json.loads(socket.recv_string()).get("result").get("status") == "ok"

        socket2 = context.socket(zmq.REQ)
        socket2.connect("tcp://ursadb:9281")
        socket2.send_string(
            'select {68656c6c6f};'
        )
        resp = json.loads(socket2.recv_string()).get("result").get("files")
        logging.info(resp)


if __name__ == "__main__":
    unittest.main()
