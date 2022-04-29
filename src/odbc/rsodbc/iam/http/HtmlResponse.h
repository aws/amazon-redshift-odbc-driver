#pragma once

#include "rs_string.h"

const rs_string ValidResponse =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 300\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html; charset=utf-8\r\n\r\n"
    "<!DOCTYPE html><html><body><p style='font: italic bold 30px Arial,sans-serif;"
    "background-color: #edde9c;color: #bc2e1e;background-color: #fff;color:#202c2d;"
    "text-shadow:0 1px #808d93,-1px 0 #cdd2d5,-3px 4px #cdd2d5;'>"
    "Thank you for using Amazon Redshift! You can now close this window.</p></body></html>";

const rs_string InvalidResponse =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Length: 95\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html; charset=utf-8\r\n\r\n"
    "<!DOCTYPE html><html><body>The request could not be understood by the server!</p></body></html>";
