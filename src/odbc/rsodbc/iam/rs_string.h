
#ifndef _RS_STRING_H_
#define _RS_STRING_H_

#include <iostream>
#include <cstdio>
#include <string>
#include <cassert>

#define btoa(x) ((x)? "true" : "false")


typedef std::string rs_string;

rs_string to_rs_string( int x );
rs_string to_rs_string( short x );
short to_short( rs_string str );
int to_int( rs_string str );

rs_string to_rs_string( bool x );

bool to_bool( rs_string str );
#endif

