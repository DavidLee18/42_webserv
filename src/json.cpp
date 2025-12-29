#include "webserv.h"
#include <cstring>

Result<MapRecord<Json *, size_t> > Json::Parser::null_or_undef(const char *raw) {
  std::string raws(raw);
  const size_t pos = raws.find("null");
  const size_t pos2 = raws.find("undefined");

  if (pos == 0)
    return OKREC(MapRecord, Json *, size_t, Json::null(), 4);
  if (pos2 == 0)
    return OKREC(MapRecord, Json *, size_t, Json::null(), 9);
  return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_boolean(const char *raw) {
  std::string raws(raw);
  const size_t pos = raws.find("true");
  const size_t pos2 = raws.find("false");

  if (pos == 0)
    return OKREC(MapRecord, Json *, size_t, Json::_bool(true), 4);
  if (pos2 == 0)
    return OKREC(MapRecord, Json *, size_t, Json::_bool(false), 5);
  return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_num(const char *raw) {
  std::stringstream ss(raw);
  long double ld;

  ss >> ld;
  if (!ss || !ss.str().empty())
    return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
  return OKREC(MapRecord, Json *, size_t, Json::num(ld), std::strlen(raw));
}

Result<MapRecord<Json *, size_t> > Json::Parser::_str(const char *raw) {
  std::string raws(raw);

  if (raws[0] != '\"')
    return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
  const size_t pos = raws.find("\"", 1);
  if (pos == std::string::npos)
    return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
  char *str = new char[pos - 1];
  str = std::strncpy(str, raw + 1, pos - 1);
  return OKREC(MapRecord, Json *, size_t, Json::str(str), pos + 1);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_arr(const char *raw) {
  std::string raws(raw);

  if (raws[0] != '[')
    return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
  Vec<Json> recs;
  for (size_t i = 1; i < raws.length();) {
    while (std::isspace(raws[i]))
      i++;
    switch ((static_cast<size_t>(recs.empty()) << 2) +
            (static_cast<size_t>(raws[i] == ',') << 1) +
            (static_cast<size_t>(raws[i] == ']'))) {
    case 1:
      Json *jss;
      jss = new Json[recs.size()];
      size_t j;
      for (j = 0; j < recs.size(); j++) {
        Result<Json *> res = recs.get(j);
        Json **jsp;
        TRYFREC(MapRecord, Json *, size_t, jsp, res, delete[] jss;)
        *(jss + (j * sizeof(Json))) = **jsp;
        delete *jsp;
      }
      return OKREC(MapRecord, Json *, size_t,
                   Json::arr((Jsons){.ptr = jss, .size = recs.size()}), i + 1);
    case 5:
      return OKREC(MapRecord, Json *, size_t,
                   Json::arr((Jsons){.ptr = NULL, .size = 0}), i + 1);
    default:
      return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
    }
    i++;
    while (std::isspace(raws[i]))
      i++;
    Result<MapRecord<Json *, size_t> > TRY_PARSE(
        null_or_undef, rec, recs, i, raw) TRY_PARSE(_boolean, rec, recs, i, raw)
        TRY_PARSE(_num, rec, recs, i, raw) TRY_PARSE(_str, rec, recs, i, raw)
            TRY_PARSE(_arr, rec, recs, i, raw)
                TRY_PARSE(_obj, rec, recs, i, raw) return ERREC(
                    MapRecord, Json *, size_t, errors::invalid_format);
  }
  return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_obj(const char *raw) {
  std::string raws(raw);

  if (raws[0] != '{')
    return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
  Vec<MapRecord<std::string, Json> > recs;
  for (size_t i = 1; i < raws.length();) {
    while (std::isspace(raws[i]))
      i++;
    switch ((static_cast<size_t>(recs.empty()) << 2) +
            (static_cast<size_t>(raws[i] == ',') << 1) +
            (static_cast<size_t>(raws[i] == ']'))) {
    case 1:
      MapRecord<std::string, Json> *rs;
      rs = reinterpret_cast<MapRecord<std::string, Json> *>(operator new(
          sizeof(MapRecord<std::string, Json>) * recs.size()));
      for (size_t j = 0; j < recs.size(); j++) {
        Result<MapRecord<std::string, Json> *> r = recs.get(j);
        MapRecord<std::string, Json> **_r;
        TRYFREC(MapRecord, Json *, size_t, _r, r, operator delete(rs);)
        *(rs + (j * sizeof(MapRecord<std::string, Json>))) = **_r;
        delete *_r;
      }
      return OKREC(MapRecord, Json *, size_t,
                   Json::obj((Map){.ptr = rs, .size = recs.size()}), i + 1);
    case 5:
      return OKREC(MapRecord, Json *, size_t,
                   Json::obj((Map){.ptr = NULL, .size = 0}), i + 1);
    default:
      return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
    }
    while (std::isspace(raws[i]))
      i++;
    Result<MapRecord<Json *, size_t> > rec = _str(raw + i);
    MapRecord<Json *, size_t> *k;
    TRYREC(MapRecord, Json *, size_t, k, rec);
    i += k->value;
    std::string _k(k->key->value._str.ptr);
    delete k->key;
    while (std::isspace(raws[i]))
      i++;
    if (raws[i] != ':')
      return (delete k->key,
              ERREC(MapRecord, Json *, size_t, errors::invalid_format));
    i++;
    while (std::isspace(raws[i]))
      i++;
    TRY_PARSE_REC(null_or_undef, _k, rec, recs, i, raw)
    TRY_PARSE_REC(_boolean, _k, rec, recs, i, raw)
    TRY_PARSE_REC(_num, _k, rec, recs, i, raw)
    TRY_PARSE_REC(_str, _k, rec, recs, i, raw)
    TRY_PARSE_REC(_arr, _k, rec, recs, i, raw)
    TRY_PARSE_REC(_obj, _k, rec, recs, i, raw)
    return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
  }
  return ERREC(MapRecord, Json *, size_t, errors::invalid_format);  
}
