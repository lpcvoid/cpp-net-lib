#pragma once

#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <algorithm>

namespace netlib::http {

using http_header_entry = std::pair<std::string, std::string>;
using http_headers = std::vector<http_header_entry>;

struct http_response {
    http_headers headers;
    uint32_t response_code;
    std::pair<uint32_t, uint32_t> version;
    std::string body;
    inline std::error_condition from_raw_response(const std::string& raw_response) {
        // a very rudimentary http response parser
        if (raw_response.empty()) {
            return std::errc::no_message;
        }

        /* strategy:
         * split into multiple (at least two) parts, delimited by \r\n\r\n"
         * within the first part:
         *      first, parse the status line
         *      second, parse the header fields, until we arrive at an empty line (only CR LF)
         *      last, an optional body
         * then, for the rest of the parts, concat into body
         */

        auto split = [](const std::string& str, const std::string& delimiter) -> std::vector<std::string> {
            std::vector<std::string> split_tokens;
            std::size_t start;
            std::size_t end = 0;
            while ((start = str.find_first_not_of(delimiter, end)) != std::string::npos)
            {
                end = str.find(delimiter, start);
                split_tokens.push_back(str.substr(start, end - start));
            }
            return split_tokens;
        };

        std::vector<std::string> header_body_split = split(raw_response, "\r\n\r\n");
        //split header part of response into response_header_lines
        std::vector<std::string> response_header_lines = split(header_body_split.front(), "\r\n");
        //first line should start with "HTTP"
        if (!response_header_lines.front().starts_with("HTTP")) {
            return std::errc::result_out_of_range;
        }
        //attempt to parse status line
        //split into parts by space
        auto status_parts = split(response_header_lines.front(), " ");
        if (status_parts.size() < 3) {
            return std::errc::bad_message;
        }
        //parse "HTTP/x.x"
        auto version_parts = split(status_parts.front(), "/");
        if (version_parts.size() != 2) {
            return std::errc::bad_message;
        }
        //parse "x.x"
        auto version_components = split(version_parts.back(), ".");
        version.first = std::stoi(version_components.front());
        version.second = std::stoi(version_components.back());
        //parse response code
        response_code = std::stoi(status_parts[1]);
        //there can be an optional code description in the first line, but we ignore that here
        //parse the response header lines until the end
        //start at second line, first is status
        std::for_each(response_header_lines.begin(), response_header_lines.end(), [&](const std::string& header_component){
            auto component_parts = split(header_component, ":");
            if (component_parts.size() == 2) {
                headers.emplace_back(component_parts.front(), component_parts.back());
            }
        });

        //now, take the body part(s) and concat them
        std::for_each(header_body_split.begin() + 1, header_body_split.end(), [&](const std::string& body_line){
            body += body_line;
        });

        return {};

    };
};

}