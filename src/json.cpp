#include "webserv.h"

Json::Json(const Json &other) : type(other.type) {
  switch (other.type) {
  case JsonNull:
    value = (JsonValue){._null = NULL};
    break;
  case JsonBool:
    value = (JsonValue){._bool = other.value._bool};
    break;
  case JsonNum:
    value = (JsonValue){.num = other.value.num};
    break;
  case JsonStr:
    value = (JsonValue){._str = new std::string(*other.value._str)};
    break;
  case JsonArr:
    value = (JsonValue){.arr = new Vec<Json>(*other.value.arr)};
    break;
  case JsonObj:
    value = (JsonValue){
        .obj = new Vec<MapRecord<std::string, Json> >(*other.value.obj)};
    break;
  }
}

Json *Json::null() { return new Json(JsonNull, (JsonValue){._null = NULL}); }

Json *Json::_bool(bool b) {
  return new Json(JsonBool, (JsonValue){._bool = b});
}

Json *Json::num(long double ld) {
  return new Json(JsonNum, (JsonValue){.num = ld});
}

Json *Json::str(std::string s) {
  return new Json(JsonStr, (JsonValue){._str = new std::string(s)});
}

Json *Json::arr(Vec<Json> js) {
  return new Json(JsonArr, (JsonValue){.arr = new Vec<Json>(js)});
}

Json *Json::obj(Vec<MapRecord<std::string, Json> > m) {
  return new Json(JsonObj,
                  (JsonValue){.obj = new Vec<MapRecord<std::string, Json> >(m)});
}

Result<MapRecord<Json *, size_t> > Json::Parser::null_or_undef(const char *raw) {
  const char *pos = std::strstr(raw, "null");
  const char *pos2 = std::strstr(raw, "undefined");

  if (pos == raw)
    return OK_REC(MapRecord, Json *, size_t, Json::null(), 4);
  if (pos2 == raw)
    return OK_REC(MapRecord, Json *, size_t, Json::null(), 9);
  return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_boolean(const char *raw) {
  const char *pos = std::strstr(raw, "true");
  const char *pos2 = std::strstr(raw, "false");

  if (pos == raw)
    return OK_REC(MapRecord, Json *, size_t, Json::_bool(true), 4);
  if (pos2 == raw)
    return OK_REC(MapRecord, Json *, size_t, Json::_bool(false), 5);
  return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_num(const char *raw,
                                                     char end) {
  char *endptr;
  errno = 0;
  long l = std::strtol(raw, &endptr, 10);
  if (endptr != raw && *endptr == end && errno != ERANGE)
    return OK_REC(MapRecord, Json *, size_t,
                  Json::num(static_cast<long double>(l)), endptr - raw);
  long double ld = std::strtold(raw, &endptr);
  if (endptr == raw || *endptr != end)
    return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
  if (errno == ERANGE)
    return ERR_REC(MapRecord, Json *, size_t, Errors::out_of_rng);
  return OK_REC(MapRecord, Json *, size_t, Json::num(ld), endptr - raw);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_str(const char *raw) {
  if (raw[0] != '\"')
    return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
  const char *pos = std::strchr(raw + 1, '\"');
  if (pos == NULL)
    return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
  char *cs = new char[pos - raw];
  cs = std::strncpy(cs, raw + 1, pos - raw - 1);
  cs[pos - raw - 1] = '\0';
  std::string str(cs);
  delete[] cs;
  return OK_REC(MapRecord, Json *, size_t, Json::str(str), pos - raw + 1);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_arr(const char *raw) {
  if (raw[0] != '[')
    return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
  Vec<Json> recs;
  const size_t rawlen = std::strlen(raw);
  for (size_t i = 1; i < rawlen;) {
    while (std::isspace(raw[i]))
      i++;
    switch ((static_cast<size_t>(recs.empty()) << 2) +
            (static_cast<size_t>(raw[i] == ',') << 1) +
            (static_cast<size_t>(raw[i] == ']'))) {
    case 1:
      return OK_REC(MapRecord, Json *, size_t, Json::arr(recs), i + 1);
    case 2:
      i++;
      break;
    case 4:
      break;
    case 5:
      return OK_REC(MapRecord, Json *, size_t, Json::arr(recs), i + 1);
    default:
      return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
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
    return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
  }
  return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_obj(const char *raw) {
  if (raw[0] != '{')
    return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
  Vec<MapRecord<std::string, Json> > recs;
  for (size_t i = 1; i < std::strlen(raw);) {
    while (std::isspace(raw[i]))
      i++;
    switch ((static_cast<size_t>(recs.empty()) << 2) +
            (static_cast<size_t>(raw[i] == ',') << 1) +
            (static_cast<size_t>(raw[i] == '}'))) {
    case 1:
      return OK_REC(MapRecord, Json *, size_t, Json::obj(recs), i + 1);
    case 2:
      i++;
      break;
    case 4:
      break;
    case 5:
      return OK_REC(MapRecord, Json *, size_t, Json::obj(recs), i + 1);
    default:
      return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
    }
    while (std::isspace(raw[i]))
      i++;
    Result<MapRecord<Json *, size_t> > rec = _str(raw + i);
    MapRecord<Json *, size_t> *k;
    TRY_REC(MapRecord, Json *, size_t, MapRecord, Json *, size_t, k, rec);
    if (k->key->ty() != Json::JsonStr)
      return (delete k->key,
              ERR_REC(MapRecord, Json *, size_t, Errors::invalid_json));
    i += k->value;
    std::string _k(*k->key->value._str);
    delete k->key;
    while (std::isspace(raw[i]))
      i++;
    if (raw[i] != ':')
      return (delete k->key,
              ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format));
    i++;
    while (std::isspace(raw[i]))
      i++;
    TRY_PARSE_REC(null_or_undef, _k, rec0, recs, i, raw)
    TRY_PARSE_REC(_boolean, _k, rec1, recs, i, raw)
    TRY_PARSE_REC_NUM(_k, rec2, recs, i, raw, ' ')
    TRY_PARSE_REC_NUM(_k, rec2_0, recs, i, raw, ',')
    TRY_PARSE_REC_NUM(_k, rec2_1, recs, i, raw, '}')
    TRY_PARSE_REC(_str, _k, rec3, recs, i, raw)
    TRY_PARSE_REC(_arr, _k, rec4, recs, i, raw)
    TRY_PARSE_REC(_obj, _k, rec5, recs, i, raw)
    return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
  }
  return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::parse(const char *raw,
                                                      char num_end) {
  Result<MapRecord<Json *, size_t> > res = null_or_undef(raw);
  if (res.error().empty() && !raw[res.value()->value])
    return OK_REC(MapRecord, Json *, size_t, res.value()->key,
                  res.value()->value);
  Result<MapRecord<Json *, size_t> > res1 = _boolean(raw);
  if (res1.error().empty() && !raw[res1.value()->value])
    return OK_REC(MapRecord, Json *, size_t, res1.value()->key,
                  res1.value()->value);
  Result<MapRecord<Json *, size_t> > res2 = _num(raw, num_end);
  if (res2.error().empty() && !raw[res2.value()->value])
    return OK_REC(MapRecord, Json *, size_t, res2.value()->key,
                  res2.value()->value);
  Result<MapRecord<Json *, size_t> > res3 = _str(raw);
  if (res3.error().empty() && !raw[res3.value()->value])
    return OK_REC(MapRecord, Json *, size_t, res3.value()->key,
                  res3.value()->value);
  Result<MapRecord<Json *, size_t> > res4 = _arr(raw);
  if (res4.error().empty() && !raw[res4.value()->value])
    return OK_REC(MapRecord, Json *, size_t, res4.value()->key,
                  res4.value()->value);
  Result<MapRecord<Json *, size_t> > res5 = _obj(raw);
  if (res5.error().empty() && !raw[res5.value()->value])
    return OK_REC(MapRecord, Json *, size_t, res5.value()->key,
                  res5.value()->value);
  return ERR_REC(MapRecord, Json *, size_t, Errors::invalid_json);
}

std::ostream &operator<<(std::ostream &os, Json &js) {
  switch (js.type) {
  case Json::JsonNull:
    os << "null";
    break;
  case Json::JsonBool:
    os << js.value._bool;
    break;
  case Json::JsonNum:
    os << js.value.num;
    break;
  case Json::JsonStr:
    os << '\"' << *js.value._str << '\"';
    break;
  case Json::JsonArr:
    os << "JsonArr " << *js.value.arr;
    break;
  case Json::JsonObj:
    os << '{';
    for (size_t i = 0; i < js.value.obj->size(); i++) {
      if (i != 0) {
        os << ", ";
      }
      Result<MapRecord<std::string, Json> *> res = js.value.obj->get(i);
      MapRecord<std::string, Json> *const *line;
      if (!res.error().empty()) {
        std::cerr << res.error() << std::endl;
        return os;
      }
      line = res.value();
      os << **line;
    }
    os << '}';
    break;
  }
  return os;
}
