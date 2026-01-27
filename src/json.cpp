#include "webserv.h"

Json::Json(const Json &other) : _type(other._type) {
  switch (other._type) {
  case Null:
    _value = (Value){._null = NULL};
    break;
  case Bool:
    _value = (Value){._bool = other._value._bool};
    break;
  case Num:
    _value = (Value){.num = other._value.num};
    break;
  case Str:
    _value = (Value){._str = new std::string(*other._value._str)};
    break;
  case Arr:
    _value = (Value){.arr = new std::vector<Json>(*other._value.arr)};
    break;
  case Obj:
    _value = (Value){.obj = new std::vector<std::pair<std::string, Json> >(
                         *other._value.obj)};
    break;
  }
}

Json& Json::operator=(const Json &other) {
  if (this != &other) {
    // Clean up existing value
    switch (_type) {
    case Str:
      delete _value._str;
      break;
    case Arr:
      delete _value.arr;
      break;
    case Obj:
      delete _value.obj;
      break;
    default:
      break;
    }
    
    // Copy new value
    _type = other._type;
    switch (other._type) {
    case Null:
      _value = (Value){._null = NULL};
      break;
    case Bool:
      _value = (Value){._bool = other._value._bool};
      break;
    case Num:
      _value = (Value){.num = other._value.num};
      break;
    case Str:
      _value = (Value){._str = new std::string(*other._value._str)};
      break;
    case Arr:
      _value = (Value){.arr = new std::vector<Json>(*other._value.arr)};
      break;
    case Obj:
      _value = (Value){.obj = new std::vector<std::pair<std::string, Json> >(
                           *other._value.obj)};
      break;
    }
  }
  return *this;
}

Json Json::null() { return Json(Null, (Value){._null = NULL}); }

Json Json::_bool(bool b) { return Json(Bool, (Value){._bool = b}); }

Json Json::num(long double ld) { return Json(Num, (Value){.num = ld}); }

Json Json::str(std::string s) {
  return Json(Str, (Value){._str = new std::string(s)});
}

Json Json::arr(std::vector<Json> js) {
  return Json(Arr, (Value){.arr = new std::vector<Json>(js)});
}

Json Json::obj(std::vector<std::pair<std::string, Json> > m) {
  return Json(
      Obj, (Value){.obj = new std::vector<std::pair<std::string, Json> >(m)});
}

Result<std::pair<Json, size_t> > Json::Parser::null_or_undef(const char *raw) {
  const char *pos = std::strstr(raw, "null");
  const char *pos2 = std::strstr(raw, "undefined");

  if (pos == raw)
    return OK_PAIR(Json, size_t, Json::null(), 4);
  if (pos2 == raw)
    return OK_PAIR(Json, size_t, Json::null(), 9);
  return ERR_PAIR(Json, size_t, Errors::invalid_format);
}

Result<std::pair<Json, size_t> > Json::Parser::_boolean(const char *raw) {
  const char *pos = std::strstr(raw, "true");
  const char *pos2 = std::strstr(raw, "false");

  if (pos == raw)
    return OK_PAIR(Json, size_t, Json::_bool(true), 4);
  if (pos2 == raw)
    return OK_PAIR(Json, size_t, Json::_bool(false), 5);
  return ERR_PAIR(Json, size_t, Errors::invalid_format);
}

Result<std::pair<Json, size_t> > Json::Parser::_num(const char *raw,
                                                     char end) {
  char *endptr;
  errno = 0;
  long l = std::strtol(raw, &endptr, 10);
  if (endptr != raw && *endptr == end && errno != ERANGE)
    return OK_PAIR(Json, size_t, Json::num(static_cast<long double>(l)),
                   endptr - raw);
  long double ld = std::strtold(raw, &endptr);
  if (endptr == raw || *endptr != end)
    return ERR_PAIR(Json, size_t, Errors::invalid_format);
  if (errno == ERANGE)
    return ERR_PAIR(Json, size_t, Errors::out_of_rng);
  return OK_PAIR(Json, size_t, Json::num(ld), endptr - raw);
}

Result<std::pair<Json, size_t> > Json::Parser::_str(const char *raw) {
  if (raw[0] != '\"')
    return ERR_PAIR(Json, size_t, Errors::invalid_format);
  const char *pos = std::strchr(raw + 1, '\"');
  if (pos == NULL)
    return ERR_PAIR(Json, size_t, Errors::invalid_format);
  char *cs = new char[static_cast<size_t>(pos - raw)];
  cs = std::strncpy(cs, raw + 1, static_cast<size_t>(pos - raw - 1));
  cs[pos - raw - 1] = '\0';
  std::string str(cs);
  delete[] cs;
  return OK_PAIR(Json, size_t, Json::str(str), pos - raw + 1);
}

Result<std::pair<Json, size_t> > Json::Parser::_arr(const char *raw) {
  if (raw[0] != '[')
    return ERR_PAIR(Json, size_t, Errors::invalid_format);
  std::vector<Json> recs;
  const size_t rawlen = std::strlen(raw);
  for (size_t i = 1; i < rawlen;) {
    while (std::isspace(raw[i]))
      i++;
    switch ((static_cast<size_t>(recs.empty()) << 2) +
            (static_cast<size_t>(raw[i] == ',') << 1) +
            (static_cast<size_t>(raw[i] == ']'))) {
    case 1:
      return OK_PAIR(Json, size_t, Json::arr(recs), i + 1);
    case 2:
      i++;
      break;
    case 4:
      break;
    case 5:
      return OK_PAIR(Json, size_t, Json::arr(recs), i + 1);
    default:
      return ERR_PAIR(Json, size_t, Errors::invalid_format);
    }
    while (std::isspace(raw[i]))
      i++;
    TRY_PARSE(null_or_undef, rec, recs, i, raw)
    TRY_PARSE(_boolean, rec1, recs, i, raw)
    TRY_PARSE_NUM(rec2_0, recs, i, raw, ' ')
    TRY_PARSE_NUM(rec2, recs, i, raw, ',')
    TRY_PARSE_NUM(rec2_1, recs, i, raw, ']')
    TRY_PARSE(_str, rec3, recs, i, raw)
    TRY_PARSE(_arr, rec4, recs, i, raw)
    TRY_PARSE(_obj, rec5, recs, i, raw)
    return ERR_PAIR(Json, size_t, Errors::invalid_format);
  }
  return ERR_PAIR(Json, size_t, Errors::invalid_format);
}

Result<std::pair<Json, size_t> > Json::Parser::_obj(const char *raw) {
  if (raw[0] != '{')
    return ERR_PAIR(Json, size_t, Errors::invalid_format);
  std::vector<std::pair<std::string, Json> > recs;
  for (size_t i = 1; i < std::strlen(raw);) {
    while (std::isspace(raw[i]))
      i++;
    switch ((static_cast<size_t>(recs.empty()) << 2) +
            (static_cast<size_t>(raw[i] == ',') << 1) +
            (static_cast<size_t>(raw[i] == '}'))) {
    case 1:
      return OK_PAIR(Json, size_t, Json::obj(recs), i + 1);
    case 2:
      i++;
      break;
    case 4:
      break;
    case 5:
      return OK_PAIR(Json, size_t, Json::obj(recs), i + 1);
    default:
      return ERR_PAIR(Json, size_t, Errors::invalid_format);
    }
    while (std::isspace(raw[i]))
      i++;
    Result<std::pair<Json, size_t> > rec = _str(raw + i);
    std::pair<Json, size_t> k;
    TRY_PAIR(Json, size_t, k, rec);
    if (k.first.type() != Json::Str)
      return ERR_PAIR(Json, size_t, Errors::invalid_json);
    i += k.second;
    std::string _k(*k.first.value()._str);
    while (std::isspace(raw[i]))
      i++;
    if (raw[i] != ':')
      return ERR_PAIR(Json, size_t, Errors::invalid_format);
    i++;
    while (std::isspace(raw[i]))
      i++;

    TRY_PARSE_PAIR(null_or_undef, _k, rec0, recs, i, raw)
    TRY_PARSE_PAIR(_boolean, _k, rec1, recs, i, raw)
    TRY_PARSE_PAIR_NUM(_k, rec2, recs, i, raw, ' ')
    TRY_PARSE_PAIR_NUM(_k, rec2_0, recs, i, raw, ',')
    TRY_PARSE_PAIR_NUM(_k, rec2_1, recs, i, raw, '}')
    TRY_PARSE_PAIR(_str, _k, rec3, recs, i, raw)
    TRY_PARSE_PAIR(_arr, _k, rec4, recs, i, raw)
    TRY_PARSE_PAIR(_obj, _k, rec5, recs, i, raw)
    return ERR_PAIR(Json, size_t, Errors::invalid_format);
  }
  return ERR_PAIR(Json, size_t, Errors::invalid_format);
}

Result<std::pair<Json, size_t> > Json::Parser::parse(const char *raw,
                                                      char num_end) {
  Result<std::pair<Json, size_t> > res = null_or_undef(raw);
  if (res.error().empty() && !raw[res.value().second])
    return OK_PAIR(Json, size_t, res.value().first, res.value().second);
  Result<std::pair<Json, size_t> > res1 = _boolean(raw);
  if (res1.error().empty() && !raw[res1.value().second])
    return OK_PAIR(Json, size_t, res1.value().first, res1.value().second);
  Result<std::pair<Json, size_t> > res2 = _num(raw, num_end);
  if (res2.error().empty() && !raw[res2.value().second])
    return OK_PAIR(Json, size_t, res2.value().first, res2.value().second);
  Result<std::pair<Json, size_t> > res3 = _str(raw);
  if (res3.error().empty() && !raw[res3.value().second])
    return OK_PAIR(Json, size_t, res3.value().first, res3.value().second);
  Result<std::pair<Json, size_t> > res4 = _arr(raw);
  if (res4.error().empty() && !raw[res4.value().second])
    return OK_PAIR(Json, size_t, res4.value().first, res4.value().second);
  Result<std::pair<Json, size_t> > res5 = _obj(raw);
  if (res5.error().empty() && !raw[res5.value().second])
    return OK_PAIR(Json, size_t, res5.value().first, res5.value().second);
  return ERR_PAIR(Json, size_t, Errors::invalid_json);
}

std::ostream &operator<<(std::ostream &os, Json &js) {
  switch (js._type) {
  case Json::Null:
    os << "null";
    break;
  case Json::Bool:
    os << js._value._bool;
    break;
  case Json::Num:
    os << js._value.num;
    break;
  case Json::Str:
    os << '\"' << *js._value._str << '\"';
    break;
  case Json::Arr:
    os << "JsonArr " << *js._value.arr;
    break;
  case Json::Obj:
    os << '{';
    for (size_t i = 0; i < js._value.obj->size(); i++) {
      if (i != 0) {
        os << ", ";
      }
      os << (*js._value.obj)[i];
    }
    os << '}';
    break;
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, std::vector<Json> &jss) {
  os << '[';
  for (size_t i = 0; i < jss.size(); i++) {
    if (i != 0) {
      os << ", ";
    }
    os << jss[i];
  }
  os << ']';
  return os;
}

std::ostream &operator<<(std::ostream &os, std::pair<std::string, Json> &rec) {
  os << '\"' << rec.first << "\": " << rec.second;
  return os;
}
