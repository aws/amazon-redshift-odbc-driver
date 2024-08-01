#pragma once

#include "rs_string.h"
#include "rslog.h"

#include <vector>
#include <unordered_map>

enum class STATE
{
    PARSE_REQUEST,
    PARSE_HEADER,
    PARSE_BODY,
    PARSE_FINISHED
};

enum class STATUS
{
    SUCCEED,
    FAILED,
    EMPTY_REQUEST
};

/*
* This class is used to parse the HTTP POST request
* and retrieve authorization code.
*/
class Parser {
    public:

        Parser();

        ~Parser() = default;

        /*
        * Initiate request parsing.
        *
        * Return true if parse was successful, otherwise false.
        */
        STATUS Parse(std::istream &in);
        
        /*
        * Check if parser is finished to parse the POST request.
        *
        * Return true if parsing was successfully finished, otherwise false.
        */
        bool IsFinished() const;
        
        /*
        * Retrieve authorization code.
        *
        * Return received authorization code or empty string.
        */
        rs_string RetrieveAuthCode(rs_string& state);
        
        /*
        * Retrieve SAML Response.
        *
        * Return received SAML Response or empty string.
        */
        rs_string RetrieveSamlResponse();
        
    private:
        /*
        * Parse received POST request line by line.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParsePostRequest(rs_string& str);
                
        /*
        * Parse request-line and perform verification for:
        * method, Request-URI and HTTP-Version.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParseRequestLine(rs_string& str);
                
        /*
        * Parse request header line.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParseHeaderLine(rs_string& str);
                
        /*
        * Parse request body line in application/x-www-form-urlencoded format.
        *
        * Return void or throw an exception if parse wasn't successful.
        */
        void ParseBodyLine(rs_string& str);

        /*
        * Expected METHOD, URI and HTTP VERSION in request line.
        */
        const rs_string METHOD = "POST";
        const rs_string URI = "/redshift/";
        const rs_string HTTP_VERSION = "HTTP/1.1";
        const int MAX_HEADER_SIZE = 8192;

        const rs_string GET_METHOD = "GET";
        const rs_string PKCE_URI = "/?code=";
                        
        STATE parser_state_;
        size_t header_size_;
        std::unordered_map<rs_string, rs_string> header_;
        std::unordered_map<rs_string, rs_string> body_;
                        
};

////////////////////////////////////////////////////////////////////////////////////////////////////
inline Parser::Parser()
    : parser_state_(STATE::PARSE_REQUEST)
    , header_size_(0)
{
    ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool Parser::IsFinished() const
{
  RS_LOG_DEBUG("IAM", "Parser.IsFinished");

    return parser_state_ == STATE::PARSE_FINISHED;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline rs_string Parser::RetrieveAuthCode(rs_string& state)
 {
  RS_LOG_DEBUG("IAM", "Parser.RetrieveAuthCode");

    if (body_.count("code") && body_.count("state") && (body_["state"] == state))
    {
        return body_["code"];
    }

    return "";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
inline rs_string Parser::RetrieveSamlResponse()
{
  RS_LOG_DEBUG("IAM", "Parser.RetrieveSamlResponse");

    return body_.count("SAMLResponse") ? body_["SAMLResponse"] : "";
}
