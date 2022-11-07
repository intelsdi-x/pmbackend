DISCONTINUATION OF PROJECT. 

This project will no longer be maintained by Intel.

This project has been identified as having known security escapes.

Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  

Intel no longer accepts patches to this project.
# PMbackend

1. Description
2. Prerequisites
3. Building the source
3. API documentation

## Description

PMBackend is small helper library providing key-value-like interface for accessing fast storage media: SSD, NVMe, NVDIMM. It was created solely in purpose as helper library for Ceph's storage backend PMStore.

## Prerequisites

* cmake
* make
* gcc or clang
* pthread
* libuuid
* git (for NVML submodule)
* gtest

## Building the sourcee

mkdir build && cd build

cmake ..

make
