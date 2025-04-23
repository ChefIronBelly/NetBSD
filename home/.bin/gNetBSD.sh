#!/usr/pkg/bin/bash
#
# Get NetBSD-10.99.* sets
#
# depends: curl and common sense

set -e
set -x

now=$(date +"%m%d%I%M""Z")
mkdir /home/chef/HEAD/${now} && cd $_
echo $now

mkdir -p /home/chef/HEAD/${now}/binary/sets && cd $_
for file in kern-GENERIC modules base comp etc gpufw man text xbase xcomp xetc xfont xserver
do
    curl -C - -LO http://nyftp.netbsd.org/pub/NetBSD-daily/HEAD/202504221750Z/amd64/binary/sets/${file}.tar.xz
done

cd ..
mkdir -p /home/chef/HEAD/${now}/binary/kernel && cd $_
curl -C - -LO http://nyftp.netbsd.org/pub/NetBSD-daily/HEAD/202504221750Z/amd64/binary/kernel/netbsd-GENERIC.gz

# sysupgrade auto /home/chef/HEAD/date foo
