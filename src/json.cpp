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
    if (!recs.empty() && raws[i] != ',') {
      if (raws[i] == ']') {
        Json *jss = new Json[recs.size() + 1];
        size_t j;
        for (j = 0; j < recs.size(); j++) {
          const Result<Json *> res = recs.get(j);
          Json **jsp;
          TRYFREC(MapRecord, Json *, size_t, jsp, res, delete[] jss;)
          *(jss + (j * sizeof(Json))) = **jsp;
        }
        return OKREC(MapRecord, Json *, size_t,
                     Json::arr((Jsons){.ptr = jss, .size = recs.size()}),
                     i + 1);
      }
      return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
    } else if (recs.empty() && raws[i] == ']')
      return OKREC(MapRecord, Json *, size_t,
                   Json::arr((Jsons){.ptr = NULL, .size = 0}), i + 1);
    while (std::isspace(raws[i]))
      i++;
    Result<MapRecord<Json *, size_t> > rec = null_or_undef(raw + i);
    if (rec.err == NULL) {
      recs.push(*rec.val->key);
      i += rec.val->value;
      continue;
    }
    rec = _boolean(raw + i);
    if (rec.err == NULL) {
      recs.push(*rec.val->key);
      i += rec.val->value;
      continue;
    }
    rec = _num(raw + i);
    if (rec.err == NULL) {
      recs.push(*rec.val->key);
      i += rec.val->value;
      continue;
    }
    rec = _str(raw + i);
    if (rec.err == NULL) {
      recs.push(*rec.val->key);
      i += rec.val->value;
      continue;
    }
    rec = _arr(raw + i);
    if (rec.err == NULL) {
      recs.push(*rec.val->key);
      i += rec.val->value;
      continue;
    }
    rec = _obj(raw + i);
    if (rec.err == NULL) {
      recs.push(*rec.val->key);
      i += rec.val->value;
      continue;
    }
    return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
  }
  return ERREC(MapRecord, Json *, size_t, errors::invalid_format);
}

Result<MapRecord<Json *, size_t> > Json::Parser::_obj(const char *raw) {}
