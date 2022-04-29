#include "rs_string.h"

rs_string to_rs_string( int x ) {
  int length = snprintf( NULL, 0, "%d", x );
  assert( length >= 0 );
  char* buf = new char[length + 1];
  snprintf( buf, length + 1, "%d", x );
  rs_string str( buf );
  delete[] buf;
  return str;
}

rs_string to_rs_string( short x ) {
  int length = snprintf( NULL, 0, "%hd", x );
  assert( length >= 0 );
  char* buf = new char[length + 1];
  snprintf( buf, length + 1, "%hd", x );
  rs_string str( buf );
  delete[] buf;
  return str;
}

short to_short( rs_string str ) {
  return atoi(str.c_str());
}

int to_int( rs_string str ) {
  return atoi(str.c_str());
}

rs_string to_rs_string( bool x ) {
  int length = 10;
  char* buf = new char[length + 1];
  snprintf( buf, length + 1, "%s", btoa(x) );
  rs_string str( buf );
  delete[] buf;
  return str;
}

bool to_bool( rs_string str ) {
  char *p = (char *)str.c_str();

  if(!p) return false;

  if(str == "1" || str == "true" || str == "TRUE")
    return true;
  else
    return false;
}
