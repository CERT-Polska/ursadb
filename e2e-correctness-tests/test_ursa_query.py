import unittest
import zmq  # type: ignore
import json
import os
import logging
import yara
from zipfile import ZipFile
import requests
import hashlib

current_path = os.path.abspath(os.path.dirname(__file__))
yara_dir = current_path + "/yararules/"
samples_dir = current_path + "/samples/"
samples_source = "https://store.tailcall.net/ursadb-samples_a9a4b06df37886c27e782db19cea587e98c63011cada4d2fd67a2ae34458aba1.zip"
downloaded_zip = "malware.zip"
zip_password = os.environ['MALWARE_PASSWORD']


class TestUrsaQuery(unittest.TestCase):
    def test_ursa_query(self):
        logging.info("Downloading samples...")
        if not os.listdir(samples_dir):
            download_samples(samples_source, samples_dir, zip_password)

        ursa_query_files = [f for f in os.listdir(yara_dir) if ".txt" in f]

        assert ursa_query_files

        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        logging.info("Connecting to UrsaDB...")
        socket.connect("tcp://0.0.0.0:9281")
        logging.info("Connected...")
        socket.send_string('index "' + samples_dir + '" ' "with [gram3, text4, hash4, wide8];")
        assert json.loads(socket.recv_string()).get("result").get("status") == "ok"

        failures = []
        for query_file in ursa_query_files:
            logging.info("---------------------------------------------------------")
            logging.info("Query for: " + str(query_file))
            with open(yara_dir + query_file) as f:
                data = f.read()

            logging.info("Query: " + str(data))
            socket.send_string(data)

            resp_files = json.loads(socket.recv_string()).get("result").get("files")
            logging.info("Files: " + str(resp_files))

            yara_matches = get_yara_matches(query_file[:-4])
            logging.info("Yara matches "+"("+str(len(yara_matches))+")"+" : " + str(yara_matches))

            if yara_matches != resp_files:
                failures.append(query_file[:-4])
                logging.info(
                        "Test failed for "
                        + str(query_file)
                        + ". "
                        + str(len(resp_files))
                        + " files found: "
                        + str(resp_files)
                    )

        if failures:
            logging.info("---------------------------------------------------------")
            logging.info("Test failed for: " + str(failures))
            logging.info("---------------------------------------------------------")
        assert not failures


def download_samples(samples_source, samples_dir, zip_password):
    r = requests.get(samples_source, allow_redirects=True)

    with open(samples_dir + downloaded_zip, 'wb') as f:
        logging.info("Downloading malware zip file ...")
        f.write(r.content)
        logging.info("Downloading finished.")
        # source_hash = hashfile(samples_source)
        # downloaded_hash = hashfile(samples_dir + downloaded_zip)
        #
        # if source_hash == downloaded_hash:
        #     logging.info(f"Both hashes are the same: {source_hash}")

    with ZipFile(samples_dir + downloaded_zip, 'r') as zip:
        zip.printdir()
        logging.info("Extracting malware zip file ...")
        zip.extractall(pwd=zip_password.encode())
        logging.info("Extracting finished.")
        os.remove(samples_dir + downloaded_zip)


def hashfile(file):
    BUF_SIZE = 65536
    sha256 = hashlib.sha256()

    with open(file, 'rb') as f:
        while True:
            data = f.read(BUF_SIZE)

            if not data:
                break

            sha256.update(data)

    return sha256.hexdigest()


def get_yara_matches(yara_rule):
    rule = yara.compile(filepath=yara_dir + yara_rule)
    samples = [f for f in os.listdir(samples_dir)]
    matches = []
    for sample in samples:
        match = rule.match(samples_dir + sample)
        if match:
            matches.append(samples_dir + sample)

    return matches


if __name__ == "__main__":
    unittest.main()

