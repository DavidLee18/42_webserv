#include "webserv.h"
#include <cctype>

Result<Json> Json::from_raw(const char *raw) { std::string raws(raw); }

Result<Json> Json::null_or_undef(const char *raw) {
  std::string raws(raw);
  if (raws == "null" || raws == "undefined")
    return OK(Json, Json::null());
  return ERR(Json, errors::unknown_char);
}

Result<Json> Json::_boolean(const char *raw) {
  std::string raws(raw);
  if (raws == "true")
    return OK(Json, Json::_bool(true));
  if (raws == "false")
    return OK(Json, Json::_bool(false));
  return ERR(Json, errors::unknown_char);
}

Result<Json> _num(const char *raw) {
  std::string raws(raw);
  long double dec = 0;
  long double sign = 1;
  if (raws.length() == 0 || raws.length() > LONG_DOUBLE_DIGITS)
    return ERR(Json, errors::too_long_num);
  if (raws[0] == '-') {
    sign = -1;
  } else if (raws[0] != '+' && !std::isdigit(raws[0])) {
    return ERR(Json, errors::invalid_format);
  }
  const size_t p = raws.find('.');
  size_t prevp = p == std::string::npos ? raws.length() - 1 : p - 1;
  // TODO
}
