#!/bin/bash
set -e

source envsetup.sh
rm -rf ${INSTALL_PATH}
source build.sh RELEASE

mlir_version="$(grep MLIR_VERSION ${BUILD_PATH}/CMakeCache.txt | cut -d "=" -f2)"
release_archive="./tpu-mlir_${mlir_version}"

rm -rf ${release_archive}*
cp -rf ${INSTALL_PATH} ${release_archive}

cp -rf ${PROJECT_ROOT}/regression ${release_archive}

# build a envsetup.sh
__envsetupfile=${release_archive}/envsetup.sh
rm -f __envsetupfile

echo "Create ${__envsetupfile}" 1>&2
more > "${__envsetupfile}" <<'//MY_CODE_STREAM'
#!/bin/bash
# set environment variable
TPUC_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

export PATH=${TPUC_ROOT}/bin:$PATH
export PATH=${TPUC_ROOT}/python/tools:$PATH
export PATH=${TPUC_ROOT}/python/utils:$PATH
export LD_LIBRARY_PATH=$TPUC_ROOT/lib:$LD_LIBRARY_PATH
export PYTHONPATH=${TPUC_ROOT}/python:$PYTHONPATH
export NNMODELS_PATH=${TPUC_ROOT}/../nnmodels
export REGRESSION_PATH=${TPUC_ROOT}/regression
//MY_CODE_STREAM

# generate readme.md
echo "Create readme.md" 1>&2
more > "${release_archive}/readme.md" <<'//MY_CODE_STREAM'
For test

1. set environment

``` bash
source ./envsetup.sh
```

2. run regression

``` bash
cd regression
 ./run.sh
```

After run regression test, all the bmodels will be in regression_out.

3. collect bmodels for runtime test

``` bash
mkdir bmodels_rttest
pushd bmodels_rttest
../prepare_bmrttest.py ../regression_out
popd bmodels_rttest
```

//MY_CODE_STREAM




tar -cvzf "tpu-mlir_${mlir_version}.tar.gz" ${release_archive}
rm -rf ${release_archive}
