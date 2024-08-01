#include "Parser.h"

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <exception>
#include <iterator>

////////////////////////////////////////////////////////////////////////////////////////////////////
void Parser::ParseRequestLine(rs_string& str)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseRequestLine");

    std::istringstream istr(str);
    std::vector<rs_string> command(
        (std::istream_iterator<rs_string>(istr)),
        std::istream_iterator<rs_string>()
    );

    if ((command.size() == 3) && (command[0] == METHOD) &&
        (command[1] == URI) && (command[2] == HTTP_VERSION))
    {
        parser_state_ = STATE::PARSE_HEADER;
    } 
    else if ((command.size() == 3) && (command[0] == GET_METHOD) &&
             (command[1].find(PKCE_URI) == 0) && (command[2] == HTTP_VERSION)) 
    {
        size_t question_mark_pos = command[1].find('?');
        rs_string parsed_body = command[1].substr(question_mark_pos + 1);
        ParseBodyLine(parsed_body);
    }
    else
    {
        throw std::runtime_error("Request line contains wrong information.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Parser::ParseHeaderLine(rs_string& str)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine");

    if (str == "\r")
    {
        parser_state_ = STATE::PARSE_BODY;

        return;
    }

    size_t ind = str.find(':');
    header_size_ += str.size();

    if ((ind == std::string::npos) || (header_size_ > MAX_HEADER_SIZE))
    {
        throw std::runtime_error("Received invalid header line.");
    }

    str.erase(std::remove_if(str.begin() + ind, str.end(), ::isspace), str.end());

    header_.insert({
        rs_string(str.begin(), str.begin() + ind),
        rs_string(str.begin() + ind + 1, str.end())
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Parser::ParseBodyLine(rs_string& str)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseBodyLine");

    auto str_begin = str.begin();
    auto str_end = str.end();

    while (true)
    {
        auto equal_it = find(str_begin, str_end, '=');
        auto ampersand_it = find(str_begin, str_end, '&');

        if (equal_it == str_end)
        {
            throw std::runtime_error("Received invalid body line.");
        }

        body_.insert({
            rs_string(str_begin, equal_it),
            rs_string(equal_it + 1, ampersand_it)
        });

        if ((equal_it == str_end) || (ampersand_it == str_end))
        {
            break;
        }
        else
        {
            str_begin = ampersand_it + 1;
        }
    }

    parser_state_ = STATE::PARSE_FINISHED;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Parser::ParsePostRequest(rs_string& str)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest");

    switch (parser_state_)
    {
        case STATE::PARSE_REQUEST:
        {
            ParseRequestLine(str);

            break;
        }

        case STATE::PARSE_HEADER:
        {
            ParseHeaderLine(str);

            break;
        }

        case STATE::PARSE_BODY:
        {
            if (!header_.count("Content-Type") || !header_.count("Content-Length") ||
                (header_["Content-Type"] != "application/x-www-form-urlencoded") ||
                (header_["Content-Length"] != std::to_string(str.size())))
            {
                throw std::runtime_error("Can't start parsing body as header contains invalid information.");
            }
            else
            {
                ParseBodyLine(str);
            }

            break;
        }

        default:
        {
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
STATUS Parser::Parse(std::istream &in)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::Parse");

    rs_string str;
    bool parse_result = true;
    bool is_line_received = false;
    
    while (getline(in, str))
    {
        is_line_received = true;

        try
        {
            ParsePostRequest(str);
        }
        catch (std::exception& e)
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::Parse %s", e.what());

            break;
        }
    }

    if (parser_state_ != STATE::PARSE_FINISHED)
    {
        return is_line_received ? STATUS::FAILED : STATUS::EMPTY_REQUEST;
    }

    return STATUS::SUCCEED;
}
