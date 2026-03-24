#!/usr/bin/env bash
source $(dirname ${BASH_SOURCE[0]})/assert.sh

PROG=`realpath $1`
UNITSNUM=${UNITSNUM:-17}
MNTDIR=`mktemp -d`

cleanup() {
    echo Remove mountpoint $MNTDIR
    fusermount -u $MNTDIR || (echo "Trying sudo..."; sudo fusermount -u $MNTDIR && echo DONE)
    rmdir $MNTDIR
}

trap cleanup EXIT INT

# Test section

echo Mount $MNTDIR by $PROG
assert $PROG --units=$UNITSNUM $MNTDIR

assert [[ -f $MNTDIR/ctrl ]]
for i in `seq 0 $((UNITSNUM-1))`; do
    DIR=$MNTDIR/unit$i
    assert [[ -d $DIR ]]
    assert [[ -f $DIR/lram ]]
    assert [[ -f $DIR/pram ]]
done

# End of test section

if [[ $RESULT -ne 0 ]]; then
    printf "\n****************FAILED****************\n"
else
    printf "\nAll is fine!\n"
fi

exit $RESULT
