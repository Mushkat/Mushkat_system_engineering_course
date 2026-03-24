set -e

RESULT=0

assert() {
    FORMAT="#${BASH_LINENO[0]} [%s]: $(echo "$@")\n"
    if eval "$@"; then
        printf "$FORMAT" PASSED
    else
        printf "$FORMAT" FAILED
        RESULT=1
    fi

    if [[ ! -z $NOSKIP && $RESULT -ne 0 ]]; then
        echo NOSKIP is present, abort...
        return 1
    fi
}
