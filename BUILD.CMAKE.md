#How to build:
cmake system expects dependencies from `MULTI_DEPS_DIRS` and `DEPS_DIRS`. They are all merged into `CMAKE_INCLUDE_PATH` and `CMAKE_LIBRARY_PATH`.

##Building on Linux:
### On a **linux** desktop that has unixodbc installed: 
-- mkdir cmake-build && cd cmake-build && cmake -DDEPS_DIRS=${DEPENDENCY_DIR} -DOPENSSL_DIR=${DEPENDENCY_DIR}  -DCMAKE_INSTALL_PREFIX=install ..
-- make -j 5
-- make install

##Important build Options:
###MULTI_DEPS_DIRS is one/more parent directory having multiple sub-folders, each of which have their own lib&include directories.

###DEPS_DIRS is one/more directory, each of which have their own lib&include directories.
  In case MULTI_DEPS_DIRS contains more than one directory, then you need to adjust `ODBC_DIR`` and `OPENSSL_DIR`` to point to the correct location.

####Note:
`MULTI_DEPS_DIRS` and `DEPS_DIRS` can be used together too. `MULTI_DEPS_DIRS` takes the priority in the path search.

####Expected dependencies directory structure:

####MULTI_DEPS_DIRS
`MULTI_DEPS_DIRS`=/PATH1;/PATH2;/PATH3
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


####DEPS_DIRS
`DEPS_DIRS`=/PATH4;/PATH5;/PATH6
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

