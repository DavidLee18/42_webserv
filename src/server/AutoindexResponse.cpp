#include "AutoindexResponse.hpp"

Result<std::string> AutoindexResponse::generate(const std::string &real_path,
                                                const std::string &req_uri,
                                                DIR *dir) {
  std::ostringstream html;

  html << "<!DOCTYPE html>\n"
       << "<html><head><meta charset=\"UTF-8\">\n"
       << "<meta name=\"viewport\" content=\"width=device-width, "
          "initial-scale=1.0\">\n"
       << "<title>Index of " << req_uri << "</title>\n"
       << "<style>\n"
       << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', "
          "Roboto, Helvetica, Arial, sans-serif; background-color: #0d1117; "
          "color: #c9d1d9; margin: 0; padding: 40px 20px; }\n"
       << ".container { max-width: 800px; margin: 0 auto; background: #161b22; "
          "border: 1px solid #30363d; border-radius: 12px; box-shadow: 0 8px "
          "24px rgba(0,0,0,0.2); overflow: hidden; }\n"
       << ".header { padding: 20px 24px; border-bottom: 1px solid #30363d; "
          "background: #21262d; }\n"
       << "h1 { margin: 0; font-size: 18px; font-weight: 600; word-break: "
          "break-all; color: #8b949e; }\n"
       << "h1 span { color: #e6edf3; }\n"
       << ".list { list-style: none; padding: 0; margin: 0; }\n"
       << ".item { border-bottom: 1px solid #21262d; }\n"
       << ".item:last-child { border-bottom: none; }\n"
       << ".entry { display: flex; align-items: center; gap: 12px; padding: "
          "14px 24px; }\n"
       << ".link { display: flex; align-items: center; flex: 1; min-width: 0; "
          "text-decoration: none; color: #58a6ff; transition: transform 0.2s "
          "ease; }\n"
       << ".link:hover { transform: translateX(4px); }\n"
       << ".icon { margin-right: 14px; font-size: 20px; width: 24px; "
          "text-align: center; flex: 0 0 24px; }\n"
       << ".delete-btn { border: none; border-radius: 8px; padding: 8px 14px; "
          "background: #da3633; color: #fff; font-size: 13px; font-weight: "
          "600; cursor: pointer; transition: transform 0.15s ease, "
          "background-color 0.15s ease; }\n"
       << ".delete-btn:hover { background: #f85149; transform: "
          "translateY(-1px); }\n"
       << ".delete-btn:active { transform: translateY(0); }\n"
       << "</style></head><body>\n"
       << "<div class=\"container\">\n"
       << "  <div class=\"header\">\n"
       << "    <h1>Index of <span>" << req_uri << "</span></h1>\n"
       << "  </div>\n"
       << "  <ul class=\"list\">\n"
       << "  <script src=\"/autoindex-delete.js\" defer></script>\n";

  dirent *entity;
  while ((entity = readdir(dir)) != NULL) {
    std::string name = entity->d_name;
    if (name == ".")
      continue;

    std::string full_item_path = real_path + name;
    Result<int> type_res = ResponseUtils::check_path_type(full_item_path);
    int type;
    if (!type_res.has_value())
      type = -1;
    else
      type = type_res.value();
    std::string icon = "📄";

    if (type == IS_DIR) {
      name += "/";
      icon = "📁";
    }
    if (name == "../")
      icon = "🔙";

    const bool deletable = (type != IS_DIR && name != "../");
    const std::string item_path = req_uri + name;

    html << "    <li class=\"item\">\n"
         << "      <div class=\"entry\">\n"
         << "        <a href=\"" << ResponseUtils::escape_html(name)
         << "\" class=\"link\">\n"
         << "          <span class=\"icon\">" << icon << "</span>\n"
         << "          <span>" << ResponseUtils::escape_html(name)
         << "</span>\n"
         << "        </a>\n";
    if (deletable) {
      html << "        <button type=\"button\" class=\"delete-btn\" "
              "data-delete-target=\""
           << ResponseUtils::escape_html(item_path) << "\">Delete</button>\n";
    }
    html << "      </div>\n"
         << "    </li>\n";
  }

  html << "  </ul>\n"
       << "</div>\n"
       << "</body></html>";

  if (closedir(dir) == -1) {
    return ERR(std::string, "cannot close directory");
  }
  return OK(std::string, html.str());
}
