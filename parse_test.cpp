#include <string>
#include <iostream>
#include <sstream>

int main() {
    std::string out = "Content-Type: text/html\nStatus: 404 Not Found\n\n<body>hi</body>";
    
    std::string headers_section;
    std::string body_section;
    size_t blank_line_pos = out.find("\r\n\r\n");

    if (blank_line_pos == std::string::npos) {
        blank_line_pos = out.find("\n\n");
        if (blank_line_pos != std::string::npos) {
            headers_section = out.substr(0, blank_line_pos);
            body_section = out.substr(blank_line_pos + 2);
        } else {
            body_section = out;
        }
    } else {
        headers_section = out.substr(0, blank_line_pos);
        body_section = out.substr(blank_line_pos + 4);
    }
    
    std::string status = "200 OK";
    // Check if Status exists
    size_t status_pos = headers_section.find("Status: ");
    if (status_pos != std::string::npos) {
        size_t end = headers_section.find("\n", status_pos);
        if (end != std::string::npos) {
            status = headers_section.substr(status_pos + 8, end - status_pos - 8);
            if (!status.empty() && status[status.length()-1] == '\r')
                status = status.substr(0, status.length()-1);
            // Optionally remove Status header from headers_section if it causes issues, but Browsers ignore it mostly.
        }
    }
    
    std::ostringstream full_resp;
    full_resp << "HTTP/1.1 " << status << "\r\n";
    if (!headers_section.empty())
        full_resp << headers_section << "\r\n";
    full_resp << "Content-Length: " << body_section.length() << "\r\n\r\n";
    full_resp << body_section;
    
    std::cout << full_resp.str() << std::endl;
    return 0;
}
