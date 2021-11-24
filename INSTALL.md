# Installation

## From pre-built package

UrsaDB is distributed in a form of pre-built Debian packages targeting Debian Buster and Ubuntu 18.04. You can get the packages from [GitHub Releases](https://github.com/CERT-Polska/ursadb/releases).

You may use this convenient one-liner to install the latest UrsaDB package along with the required dependencies:
```
curl https://raw.githubusercontent.com/CERT-Polska/ursadb/master/contrib/install_deb.sh | sudo bash
```

## From nixpkgs

```
nix-env -i ursadb
```

## From dockerhub

Change [index_dir] and [samples_dir] to paths on your filesystem where you want to keep
index and samples.

```
sudo docker run -v [index_dir]:/var/lib/ursadb:rw -v [samples_dir]:/mnt/samples certpl/ursadb
```

## From dockerfile

```
git clone https://github.com/CERT-Polska/ursadb.git
sudo docker image build -t ursadb .
sudo docker run -v [index_dir]:/var/lib/ursadb:rw -v [samples_dir]:/mnt/samples ursadb
```

## From source

1. Clone the repository:
```
git clone --recurse-submodules https://github.com/CERT-Polska/ursadb.git
```

2. Install necessary dependencies:
```
sudo apt update
sudo apt install -y gcc-7 g++-7 libzmq3-dev cmake build-essential clang-format git
```

3. Build project:
```
mkdir build
cd build
cmake -D CMAKE_C_COMPILER=gcc-7 -D CMAKE_CXX_COMPILER=g++-7 -D CMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

4. (Optional) Install binaries to `/usr/local/bin`:
```
sudo make install
```

5. (Optional) Consider registering UrsaDB as a systemd service:
```
cp contrib/systemd/ursadb.service /etc/systemd/system/
systemctl enable ursadb
```
