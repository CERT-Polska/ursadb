#!/bin/sh -e

apt-get update
apt-get install -y libzmq5

DEB_URL=$(curl -s https://api.github.com/repos/CERT-Polska/ursadb/releases/latest | grep browser_download_url | cut -f4 '-d"')
curl -L -o /tmp/ursadb.deb -- "$DEB_URL"
dpkg -i /tmp/ursadb.deb
rm /tmp/ursadb.deb
