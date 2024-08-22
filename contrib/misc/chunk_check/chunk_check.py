#!/usr/bin/env python3
#
# input: pool -> chunk size
# query bconsole for all volumes of the given pool
# s3wrapper gives list 'chunk-number size'
# check complete size with volbytes
# and check sequential chunks and that all but last chunk are full
# ignoring purged/error volumes

import bareos.bsock
import argparse
from subprocess import check_output

parser = argparse.ArgumentParser(
    prog='check-vol',
    description = 'Checks your s3 volumes for inconsistencies',
)

parser.add_argument('--dir_address', required=True)
parser.add_argument('--dir_port', required=True, type=int)
parser.add_argument('--console_user', required=True)
parser.add_argument('--console_pw', required=True)
parser.add_argument('--pool', required=True, help="The pool that contains the volumes that are to be checked")
parser.add_argument('--chunk_size', required=True, type=int, help="The maximum chunk size (as configured in your device configuration)")
parser.add_argument('--s3cmd', default="s3cmd", help="This will be called as '<s3cmd> list <volume>' and is expected to return a properly formatted chunk list")

args = parser.parse_args()

def get_chunks_str(name):
    b = check_output([args.s3cmd, 'list', name])
    return b.decode('utf-8')

def parse_chunk_str(s):
    parsed = []
    for line in s.strip().splitlines():
        idx, size = line.split(' ', 2)
        parsed.append([int(idx), int(size)])

    return parsed

def get_chunks(volume):
    s = get_chunks_str(volume["volumename"])
    return parse_chunk_str(s)

def check_volume(volume, size):
    chunks = get_chunks(volume)
    count = len(chunks)
    vol_size = int(volume["volbytes"])

    total_size = 0
    for idx, chunk in enumerate(chunks):
        chunk_idx, chunk_size = chunk
        if idx != chunk_idx:
            print(f"  ERROR: chunk {idx} has index {chunk_idx}.  Missing chunk ?")
            return False

        if chunk_size > size:
            print(f"  ERROR: chunk {idx} is too big (Have: {chunk_size}; max: {size}).")
            return False

        if idx + 1 < count:
            if chunk_size < size:
                print(f"  ERROR: interior chunk {idx} is too small (Have: {chunk_size}; min: {size}).")
                return False

        total_size += chunk_size

    if total_size != vol_size:
        print(f"  ERROR: total chunk bytes is not correct. (Have: {total_size}; wanted: {vol_size}).")
        return False

    return True

director = bareos.bsock.DirectorConsoleJson(
    address=args.dir_address,
    port=args.dir_port,
    name=args.console_user,
    password=args.console_pw,
)

result = director.call(f"list volumes pool={args.pool}")

if result is None:
    raise Exception('list volumes returned nothing')

bad_volumes = []

volumes = result["volumes"]

def progress(current, fin):
    max_chars = len(str(fin))
    s = str(current+1).rjust(max_chars)
    return f"{s}/{fin}"

for idx, volume in enumerate(volumes):
    name = volume["volumename"]
    print(f"({progress(idx, len(volumes))}) Checking volume {name}")
    status = volume["volstatus"]
    if status == "Error" or status == "Purged":
        print(f"  Skipping because of status = {status} ...")
        continue

    if not check_volume(volume, args.chunk_size):
        bad_volumes.append(volume)
    else:
        print("  OK")

print(f"\n\nFound {len(bad_volumes)} corrupted volumes")
for volume in bad_volumes:
    print(volume["volumename"])
