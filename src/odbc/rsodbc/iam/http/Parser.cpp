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
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseRequestLine - Input: [%s]", str.c_str());

    std::istringstream istr(str);
    std::vector<rs_string> command(
        (std::istream_iterator<rs_string>(istr)),
        std::istream_iterator<rs_string>()
    );

    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseRequestLine - Parsed command parts: %zu", command.size());
    for (size_t i = 0; i < command.size(); i++)
    {
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseRequestLine - command[%zu]: [%s]", i, command[i].c_str());
    }

    if ((command.size() == 3) && (command[0] == METHOD) &&
        (command[1] == URI) && (command[2] == HTTP_VERSION))
    {
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseRequestLine - Matched POST request, moving to PARSE_HEADER");
        parser_state_ = STATE::PARSE_HEADER;
    }
    else if ((command.size() == 3) && (command[0] == GET_METHOD) &&
             (command[1].find(PKCE_URI) == 0) && (command[2] == HTTP_VERSION))
    {
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseRequestLine - Matched GET request (PKCE)");
        size_t question_mark_pos = command[1].find('?');
        rs_string parsed_body = command[1].substr(question_mark_pos + 1);
        ParseBodyLine(parsed_body);
    }
    else
    {
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseRequestLine - Request line validation failed");
        throw std::runtime_error("Request line contains wrong information.");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Parser::ParseHeaderLine(rs_string& str)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine - Input: [%s]", str.c_str());

    if (str == "\r")
    {
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine - Found empty line (\\r), moving to PARSE_BODY");
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine - Headers collected: %zu", header_.size());
        for (auto& h : header_)
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine -   %s: %s", h.first.c_str(), h.second.c_str());
        }
        parser_state_ = STATE::PARSE_BODY;

        return;
    }

    size_t ind = str.find(':');
    header_size_ += str.size();

    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine - Colon position: %zu, header_size: %zu", ind, header_size_);

    if ((ind == std::string::npos) || (header_size_ > MAX_HEADER_SIZE))
    {
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine - Invalid header: ind=%zu, header_size=%zu (max=%d)",
                     ind, header_size_, MAX_HEADER_SIZE);
        throw std::runtime_error("Received invalid header line.");
    }

    str.erase(std::remove_if(str.begin() + ind, str.end(), ::isspace), str.end());

    rs_string key(str.begin(), str.begin() + ind);
    rs_string value(str.begin() + ind + 1, str.end());

    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseHeaderLine - Inserting header: [%s]: [%s]", key.c_str(), value.c_str());

    header_.insert({ key, value });
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Parser::ParseBodyLine(rs_string& str)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseBodyLine - Input size: %zu, content: [%s]", str.size(), str.c_str());

    auto str_begin = str.begin();
    auto str_end = str.end();

    while (true)
    {
        auto equal_it = find(str_begin, str_end, '=');
        auto ampersand_it = find(str_begin, str_end, '&');

        if (equal_it == str_end)
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParseBodyLine - ERROR: No '=' found in body line");
            throw std::runtime_error("Received invalid body line.");
        }

        rs_string key(str_begin, equal_it);
        rs_string value(equal_it + 1, ampersand_it);

        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseBodyLine - Extracted: [%s] = [%s]", key.c_str(), value.c_str());

        body_.insert({ key, value });

        if ((equal_it == str_end) || (ampersand_it == str_end))
        {
            break;
        }
        else
        {
            str_begin = ampersand_it + 1;
        }
    }

    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseBodyLine - Body parsed successfully, total fields: %zu", body_.size());
    for (auto& b : body_)
    {
        RS_LOG_DEBUG("IAMHTTP", "Parser::ParseBodyLine -   %s: %s", b.first.c_str(), b.second.c_str());
    }

    RS_LOG_DEBUG("IAMHTTP", "Parser::ParseBodyLine - Setting parser_state to PARSE_FINISHED");
    parser_state_ = STATE::PARSE_FINISHED;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Parser::ParsePostRequest(rs_string& str)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Current state: %d, input: [%s]",
                 static_cast<int>(parser_state_), str.c_str());

    switch (parser_state_)
    {
        case STATE::PARSE_REQUEST:
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Processing request line");
            ParseRequestLine(str);

            break;
        }

        case STATE::PARSE_HEADER:
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Processing header line");
            ParseHeaderLine(str);

            break;
        }

        case STATE::PARSE_BODY:
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Processing body");
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Body string size: %zu", str.size());

            bool has_content_type = header_.count("Content-Type") > 0;
            bool has_content_length = header_.count("Content-Length") > 0;
            rs_string content_type = has_content_type ? header_["Content-Type"] : "(missing)";
            rs_string content_length = has_content_length ? header_["Content-Length"] : "(missing)";

            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Content-Type: [%s]", content_type.c_str());
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Content-Length: [%s]", content_length.c_str());
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Expected Content-Length: %zu", str.size());

            if (!has_content_type || !has_content_length ||
                (header_["Content-Type"] != "application/x-www-form-urlencoded") ||
                (header_["Content-Length"] != std::to_string(str.size())))
            {
                RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Body validation FAILED:");
                RS_LOG_DEBUG("IAMHTTP", "  - has_content_type: %d", has_content_type);
                RS_LOG_DEBUG("IAMHTTP", "  - has_content_length: %d", has_content_length);
                RS_LOG_DEBUG("IAMHTTP", "  - content_type match: %d",
                             has_content_type && (header_["Content-Type"] == "application/x-www-form-urlencoded"));
                RS_LOG_DEBUG("IAMHTTP", "  - content_length match: %d",
                             has_content_length && (header_["Content-Length"] == std::to_string(str.size())));

                throw std::runtime_error("Can't start parsing body as header contains invalid information.");
            }
            else
            {
                RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - Body validation passed, parsing body line");
                ParseBodyLine(str);
            }

            break;
        }

        default:
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - In default/finished state");
            break;
        }
    }

    RS_LOG_DEBUG("IAMHTTP", "Parser::ParsePostRequest - After processing, state is now: %d",
                 static_cast<int>(parser_state_));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
STATUS Parser::Parse(std::istream &in)
{
    RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Starting parse, initial state: %d", static_cast<int>(parser_state_));

    rs_string str;
    bool parse_result = true;
    bool is_line_received = false;
    int line_count = 0;

    while (getline(in, str))
    {
        line_count++;
        is_line_received = true;

        RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Line %d received (length=%zu): [%s]",
                     line_count, str.size(), str.c_str());

        try
        {
            ParsePostRequest(str);
        }
        catch (std::exception& e)
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Exception on line %d: %s", line_count, e.what());
            RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Final parser state: %d", static_cast<int>(parser_state_));

            break;
        }

        if (parser_state_ == STATE::PARSE_FINISHED)
        {
            RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Parsing completed successfully after %d lines", line_count);
            break;
        }
    }

    RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Finished reading lines, total: %d", line_count);
    RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Final parser state: %d", static_cast<int>(parser_state_));
    RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - is_line_received: %d", is_line_received);

    if (parser_state_ != STATE::PARSE_FINISHED)
    {
        STATUS status = is_line_received ? STATUS::FAILED : STATUS::EMPTY_REQUEST;
        RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Returning status: %d", static_cast<int>(status));
        return status;
    }

    RS_LOG_DEBUG("IAMHTTP", "Parser::Parse - Returning STATUS::SUCCEED");
    return STATUS::SUCCEED;
}
