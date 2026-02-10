#ifndef HTTP_1_1_H
#define HTTP_1_1_H

#include "json.h"
#include <map>
#include <string>

class Http {
  virtual void phantom() = 0;

public:
  enum Method { GET, HEAD, OPTIONS, POST, DELETE, PUT, CONNECT, TRACE, PATCH };

  class PartialString {
  public:
    enum Type { Partial, Full };
    static PartialString partial(char *input) {
      PartialString p;
      p._part = input;
      return p;
    }
    static PartialString full(char *input) {
      PartialString p;
      p._kind = PartialString::Full;
      p._part = input;
      return p;
    }
    Type const &kind() const { return _kind; }
    Type &kind_mut() { return _kind; }
    char const *part() { return _part; }
    char *part_mut() { return _part; }

  private:
    Type _kind;
    char *_part;

    PartialString() : _kind(Partial), _part(NULL) {}
  };

  class Body {
  public:
    enum Type { Empty, HttpJson, HttpFormUrlEncoded, Html };
    union Value {
      void *_null;
      Json *json;
      std::map<std::string, std::string> *form;
      std::string *html_raw;
    };
    Body(Type t, Value v) : _type(t), _value(v) {}
    Body(const Body &other) : _type(other._type) {
      switch (_type) {
      case Empty:
        _value._null = NULL;
        break;
      case HttpJson:
        _value.json = new Json(*other._value.json);
        break;
      case HttpFormUrlEncoded:
        _value.form =
            new std::map<std::string, std::string>(*other._value.form);
        break;
      case Html:
        _value.html_raw = new std::string(*other._value.html_raw);
        break;
      }
    }
    Body &operator=(const Body &other) {
      if (this != &other) {
        // Clean up existing value
        switch (_type) {
        case HttpJson:
          delete _value.json;
          break;
        case HttpFormUrlEncoded:
          delete _value.form;
          break;
        case Html:
          delete _value.html_raw;
          break;
        default:
          break;
        }

        // Copy new value
        _type = other._type;
        switch (other._type) {
        case Empty:
          _value._null = NULL;
          break;
        case HttpJson:
          _value.json = new Json(*other._value.json);
          break;
        case HttpFormUrlEncoded:
          _value.form =
              new std::map<std::string, std::string>(*other._value.form);
          break;
        case Html:
          _value.html_raw = new std::string(*other._value.html_raw);
          break;
        }
      }
      return *this;
    }
    ~Body() {
      switch (_type) {
      case HttpJson:
        delete _value.json;
        break;
      case HttpFormUrlEncoded:
        delete _value.form;
        break;
      case Html:
        delete _value.html_raw;
        break;
      default:
        break;
      }
    }
    static Body empty() { return Body(Empty, (Value){._null = NULL}); }
    static Body json(Json j) {
      return Body(HttpJson, (Value){.json = new Json(j)});
    }
    const Type &type() const { return _type; }
    const Value &value() const { return _value; }

  private:
    Type _type;
    Value _value;
  };

  class Request {
    Method _method;
    std::map<std::string, std::string> _headers;
    std::string _path;
    Body _body;

  public:
    class Parser {
      virtual void phantom() = 0;

      static Result<std::pair<Request, size_t> > parse_request_line(const char *,
                                                                   size_t);
      static Result<std::pair<Method, size_t> > parse_method(const char *,
                                                            size_t);
      static Result<std::pair<std::string, size_t> > parse_path(const char *,
                                                               size_t);
      static Result<std::pair<std::string, size_t> >
      parse_http_version(const char *, size_t);
      static Result<std::pair<std::map<std::string, std::string>, size_t> >
      parse_headers(const char *, size_t);
      static Result<std::pair<Body, size_t> >
      parse_body(const char *, size_t,
                 std::map<std::string, std::string> const &);

    public:
      static Result<std::pair<Request, size_t> > parse(const char *, size_t);
    };

    friend class Parser;

    Request(Method m, std::string p, Body b)
        : _method(m), _headers(), _path(p), _body(b) {}
    Request(const Request &other)
        : _method(other._method), _headers(other._headers), _path(other._path),
          _body(other._body) {}
    Request &operator=(const Request &other) {
      if (this != &other) {
        _method = other._method;
        _headers = other._headers;
        _path = other._path;
        _body = other._body;
      }
      return *this;
    }
    const Method &method() const { return _method; }
    const std::map<std::string, std::string> &headers() const {
      return _headers;
    }
    const std::string &path() const { return _path; }
    const Body &body() const { return _body; }
    static Result<std::pair<Request *, size_t> > parse(const char *, char);
  };

  class Response {
    int _status_code;
    std::map<std::string, std::string> _headers;
    Body _body;

  public:
    Response(int status, std::map<std::string, std::string> headers, Body b)
        : _status_code(status), _headers(headers), _body(b) {}
    Response(const Response &other)
        : _status_code(other._status_code), _headers(other._headers),
          _body(other._body) {}
    Response &operator=(const Response &other) {
      if (this != &other) {
        _status_code = other._status_code;
        _headers = other._headers;
        _body = other._body;
      }
      return *this;
    }

    const int &status_code() const { return _status_code; }
    const std::map<std::string, std::string> &headers() const {
      return _headers;
    }
    const Body &body() const { return _body; }

    std::map<std::string, std::string> &headers_mut() { return _headers; }
    void set_header(const std::string &name, const std::string &value) {
      _headers[name] = value;
    }
  };
};
#endif
