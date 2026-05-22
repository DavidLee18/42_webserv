#ifndef MULTIPART_PARSER_HPP
#define MULTIPART_PARSER_HPP

#include <cstddef>
#include <string>

class MultipartParser {
public:
  static std::string extract_boundary(const std::string &content_type);
  static std::size_t
  parse_multipart_part(const std::string &body, const std::string &boundary,
                       const std::size_t start_pos, std::string &out_filename,
                       std::string &out_fieldname, std::string &out_data);
};

#endif
