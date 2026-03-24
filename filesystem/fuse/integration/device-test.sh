#!/usr/bin/env bash
source $(dirname ${BASH_SOURCE[0]})/assert.sh

PROG=`realpath $1`
UNITSNUM=${UNITSNUM:-9}
MNTDIR=`mktemp -d`

cleanup() {
    echo Remove mountpoint $MNTDIR
    fusermount -u $MNTDIR || (echo "Trying sudo..."; sudo fusermount -u $MNTDIR && echo DONE)
    rmdir $MNTDIR
    exit $RESULT
}

trap cleanup EXIT INT

# Test section

echo Mount $MNTDIR by $PROG
assert $PROG --units=$UNITSNUM $MNTDIR

assert echo 0 '>' $MNTDIR/ctrl
assert echo 0 '|' diff - $MNTDIR/ctrl
assert echo 5 '>' $MNTDIR/ctrl
assert echo 3 '>' $MNTDIR/ctrl
assert echo 6 '>' $MNTDIR/ctrl
assert printf '5\\n3\\n6\\n' '|' diff - $MNTDIR/ctrl

assert echo 123 '>' $MNTDIR/unit7/lram
assert echo 321 '>' $MNTDIR/unit7/pram
assert echo 123 '|' diff - $MNTDIR/unit7/lram
assert echo 321 '|' diff - $MNTDIR/unit7/pram

# End of test section

if [[ $RESULT -ne 0 ]]; then
    printf "\n****************FAILED****************\n"
else
    printf "\nAll is fine!\n"
fi

exit $RESULT
