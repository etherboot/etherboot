#!/bin/bash

# verify that atftp and atftpd are runnable
if [ ! -x /usr/bin/atftp ]; then
	echo "atftp not found"
	exit 1
fi
if [ ! -x /usr/sbin/in.tftpd ]; then
	echo "in.tftpd not found"
	exit 1
fi

# make sure we have /tftpboot with some files
if [ ! -d /tftpboot ]; then
	echo "create /tftpboot before running this test"
	exit 1
fi
READ_0=READ_0.bin
READ_511=READ_511.bin
READ_512=READ_512.bin
READ_2K=READ_2K.bin
READ_BIG=READ_BIG.bin
WRITE=write.bin

echo -n "Creating test files ... "
touch /tftpboot/$READ_0
touch /tftpboot/$WRITE; chmod a+w /tftpboot/$WRITE
dd if=/dev/urandom of=/tftpboot/$READ_511 bs=1 count=511 2>/dev/null
dd if=/dev/urandom of=/tftpboot/$READ_512 bs=1 count=512 2>/dev/null
dd if=/dev/urandom of=/tftpboot/$READ_2K bs=1 count=2048 2>/dev/null
dd if=/dev/urandom of=/tftpboot/$READ_BIG bs=1 count=51111 2>/dev/null
echo "done"

function check_file() {
	if cmp $1 $2 ; then
		echo OK
	else
		echo ERROR
	fi
}

function test_get_put() {
    echo -n "Testing get, $1 ($2)... "
    atftp $2 --get -r $1 -l out localhost 2>/dev/null
    check_file /tftpboot/$1 out
    echo -n "Testing put, $1 ($2)... "
    atftp $2 --put -r $WRITE -l out localhost 2>/dev/null
    check_file /tftpboot/$WRITE out
}

#
# test get and put
#

test_get_put $READ_0
test_get_put $READ_511
test_get_put $READ_512
test_get_put $READ_2K
test_get_put $READ_BIG

# testing for blocksize
test_get_put $READ_BIG "-b 1428"

rm -f out
