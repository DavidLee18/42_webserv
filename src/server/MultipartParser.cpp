#include "MultipartParser.hpp"
#include <string>

std::string MultipartParser::extract_boundary(const std::string &content_type) {
  size_t boundary_pos = content_type.find("boundary=");
  if (boundary_pos == std::string::npos)
    return "";

  boundary_pos += 9;
  size_t end_pos = content_type.find(';', boundary_pos);
  if (end_pos == std::string::npos)
    end_pos = content_type.find('\r', boundary_pos);
  if (end_pos == std::string::npos)
    end_pos = content_type.find('\n', boundary_pos);
  if (end_pos == std::string::npos)
    end_pos = content_type.length();

  return content_type.substr(boundary_pos, end_pos - boundary_pos);
}

std::size_t MultipartParser::parse_multipart_part(const std::string &body,
                                                  const std::string &boundary,
                                                  const std::size_t start_pos,
                                                  std::string &out_filename,
                                                  std::string &out_fieldname,
                                                  std::string &out_data) {
  out_filename = "";
  out_fieldname = "";
  out_data = "";

  const std::string boundary_marker = "--" + boundary;
  size_t part_start = body.find(boundary_marker, start_pos);

  if (part_start == std::string::npos)
    return std::string::npos;

  part_start += boundary_marker.length();
  if (part_start < body.length() && body[part_start] == '\r')
    part_start++;
  if (part_start < body.length() && body[part_start] == '\n')
    part_start++;

  size_t header_end = body.find("\r\n\r\n", part_start);
  size_t skip_length = 4;
  if (header_end == std::string::npos) {
    header_end = body.find("\n\n", part_start);
    skip_length = 2;
  }

  if (header_end == std::string::npos)
    return std::string::npos;

  std::string headers_section =
      body.substr(part_start, header_end - part_start);

  const size_t disp_pos = headers_section.find("Content-Disposition:");
  if (disp_pos != std::string::npos) {
    size_t line_end = headers_section.find('\n', disp_pos);
    if (line_end == std::string::npos)
      line_end = headers_section.length();

    std::string disp_line =
        headers_section.substr(disp_pos, line_end - disp_pos);

    size_t name_pos = disp_line.find("name=\"");
    if (name_pos != std::string::npos) {
      name_pos += 6;
      const size_t name_end = disp_line.find('"', name_pos);
      if (name_end != std::string::npos) {
        out_fieldname = disp_line.substr(name_pos, name_end - name_pos);
      }
    }

    size_t filename_pos = disp_line.find("filename=\"");
    if (filename_pos != std::string::npos) {
      filename_pos += 10;
      const size_t filename_end = disp_line.find('"', filename_pos);
      if (filename_end != std::string::npos) {
        out_filename =
            disp_line.substr(filename_pos, filename_end - filename_pos);
      }
    }
  }

  const size_t body_start = header_end + skip_length;

  size_t next_boundary = body.find("\r\n--" + boundary, body_start);
  if (next_boundary == std::string::npos)
    next_boundary = body.find("\n--" + boundary, body_start);

  if (next_boundary == std::string::npos)
    next_boundary = body.find("--" + boundary, body_start);

  if (next_boundary == std::string::npos) {
    out_data = body.substr(body_start);
    if (out_data.length() >= 2 &&
        out_data.substr(out_data.length() - 2) == "\r\n")
      out_data = out_data.substr(0, out_data.length() - 2);
    else if (!out_data.empty() && out_data[out_data.length() - 1] == '\n')
      out_data = out_data.substr(0, out_data.length() - 1);
    return std::string::npos;
  }

  out_data = body.substr(body_start, next_boundary - body_start);
  if (out_data.length() >= 2 &&
      out_data.substr(out_data.length() - 2) == "\r\n")
    out_data = out_data.substr(0, out_data.length() - 2);
  else if (!out_data.empty() && out_data[out_data.length() - 1] == '\n')
    out_data = out_data.substr(0, out_data.length() - 1);

  return next_boundary;
}
