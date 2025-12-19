#How to build:
cmake system expects dependencies from `RS_MULTI_DEPS_DIRS` and `RS_DEPS_DIRS`. They are all merged into `CMAKE_INCLUDE_PATH` and `CMAKE_LIBRARY_PATH`.

##Building on Linux:
### On a **linux** and **macOSX**: 
-- mkdir cmake-build && cd cmake-build && cmake -DRS_MULTI_DEPS_DIRS=${RS_MULTI_DEPS_DIRS} -DOPENSSL_DIR=${RS_MULTI_DEPS_DIRS}/openssl  -DRS_ODBC_DIR=${RS_MULTI_DEPS_DIRS}/unixodbc -DCMAKE_INSTALL_PREFIX=install ..
-- make -j 5
-- make install

##Important build Options:
###RS_MULTI_DEPS_DIRS is one/more parent directory having multiple sub-folders, each of which have their own lib&include directories.

###RS_DEPS_DIRS is one/more directory, each of which have their own lib&include directories.
  In case RS_MULTI_DEPS_DIRS contains more than one directory, then you need to adjust `RS_DEPS_DIRS` instead to point to the correct location.

####Note:
`RS_MULTI_DEPS_DIRS` and `RS_DEPS_DIRS` can be used together too. `RS_MULTI_DEPS_DIRS` takes the priority in the path search.

####Expected dependencies directory structure:

####RS_MULTI_DEPS_DIRS
`RS_MULTI_DEPS_DIRS`=/PATH1;/PATH2;/PATH3
where each PATHx contains something like:
PATH1
  |
  -- ssl
  |   |
  |   --- include
  |   |
  |   --- lib
  |
  -- aws-sdk-redshift
      |
      --- include
      |
      --- lib
PATH2
  |
  -- unixodbc
  |   |
  |   --- include
  |   |
  |   --- lib
  |
  -- zlib
      |
      --- include
      |
      --- lib


####RS_DEPS_DIRS
`RS_DEPS_DIRS`=/PATH4;/PATH5;/PATH6
where each PATHx contains something like:
PATH3
  |
  --- include
  |
  --- lib

PATH4
  |
  --- include
  |
  --- lib

