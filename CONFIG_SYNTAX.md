## Config File Rule 정리

현재 구현된 `WebserverConfig`, `ServerConfig`, `RouteRule_CGI`, `PathPattern` 기준으로 config 파일 문법과 규칙을 정리한다.

---

## 1. 최상위 구성

config 파일 최상위에서는 아래 항목들만 허용한다.

- `types =`
- `uwsgi =`
- `! ...` : 전역 기본 에러 페이지 설정
- `:PORT =` : 서버 블록 시작

그 외 형식은 invalid line 으로 처리한다.

---

## 2. 들여쓰기 규칙

들여쓰기는 문법의 일부로 사용된다.

- 최상위 항목: indent level 0
- 서버 블록 내부 항목: indent level 1
- route / CGI block 내부 세부 항목: indent level 2
- 서버 블록의 끝은 빈 줄이 2번 연속 나오는 것으로 판단한다.
- 항목 사이에는 빈 줄로 구분할 수 있다.

또한 줄 끝에 공백(space/tab)이 있으면 syntax error로 처리될 수 있다.

---

## 3. `types` 블록

MIME type 매핑을 정의한다.

### 형식
```conf
types =
  html|htm -> text/html
  css -> text/css
  js -> application/javascript
  _ -> application/octet-stream
```

### 규칙
- `types =` 또는 `types=` 형식으로 시작한다.
- `확장자 -> MIME 타입` 형식으로 작성한다.
- 여러 확장자는 `|` 로 연결할 수 있다.
- `_` 는 기본 MIME 타입을 의미한다.
- 같은 확장자를 중복 선언할 수 없다.
- 기본 MIME 타입(`_`)은 반드시 한 번 정의되어야 한다.
- MIME 타입은 `type/subtype` 형식이어야 한다.

---

## 4. `uwsgi` 블록

uWSGI용 Python 파일과 포트를 매핑한다.

### 형식
```conf
uwsgi =
  app.py:8000
  admin.py:8001
```

### 규칙
- `파일경로:포트` 형식으로 작성한다.
- 파일은 `.py` 여야 한다.
- 포트는 숫자만 허용한다.
- 중복 key는 허용하지 않는다.

---

## 5. 전역 기본 에러 페이지

전역 기본 에러 페이지는 `!` 로 시작한다.

### 형식
```conf
! 404:/errors/404.html
! $/usr/bin/python(ERROR_MODE=default)
```

### 규칙
- `!` 다음에는 값이 하나만 와야 한다.
- `$` 로 시작하지 않으면 `상태코드:경로` 형식으로 해석한다.
- `$` 로 시작하면 CGI executable 설정으로 해석한다.

---

## 6. 서버 블록

서버 블록은 포트 번호를 기준으로 선언한다.

### 형식
```conf
:8080 =
```

### 규칙
- `:PORT =` 형식이어야 한다.
- `PORT` 는 숫자만 가능하다.
- 같은 포트를 중복 선언할 수 없다.

---

## 7. 서버 블록 내부 허용 항목

서버 블록 내부에서는 정해진 항목만 사용할 수 있다.

### 형식
```conf
:8080 =
  [] +<= Server: webserv

  ...3

  $add_csp_sha256.cgi

  GET / <- /var/www/html

  GET /cgi-bin/test $/usr/bin/python(SCRIPT_MODE=prod)
```

### 규칙
- Header 설정을 사용할 수 있다.
- Server response time을 설정할 수 있다.
- Server CGI shortcut을 선언할 수 있다.
- Route rule을 선언할 수 있다.
- Route CGI config를 선언할 수 있다.
- 서버 블록의 끝은 빈 줄이 2번 연속 나오는 것으로 판단한다.
- 그 외 형식은 invalid line으로 처리한다.

---

## 8. Header 설정

특수 문법으로 header를 추가한다.

### 형식
```conf
[] +<= Content-Type: text/html
[] +<= X-Test: hello;
    world
```

### 규칙
- 총 4개 토큰이어야 한다.
- 첫 토큰은 `[]` 여야 한다.
- 둘째 토큰은 `+<=` 여야 한다.
- 셋째 토큰은 `:` 로 끝나야 한다.
- 넷째 토큰에는 값이 있어야 한다.
- 값이 `;` 로 끝나면 다음 줄을 이어서 읽는다.
- 이어지는 줄은 indent level 2여야 한다.

---

## 9. Server response time

서버 응답 시간을 설정한다.

### 형식
```conf
...3
```

### 규칙
- `...` 으로 시작해야 한다.
- 뒤에는 숫자만 허용한다.
- 범위는 `1 ~ 900` 이다.

---

## 10. Route Rule

서버 블록 내부의 라우팅 규칙은 아래 형식을 따른다.

### 형식
```conf
GET / <- /var/www/html
GET|POST /upload -> /data/upload
GET /docs =301> /documents
GET /list <i- /var/www/list
```

### 규칙
- 총 4개 토큰이어야 한다.
- METHOD 는 `GET`, `POST`, `DELETE` 만 허용한다.
- 여러 method는 `|` 로 연결할 수 있다.
- operator는 정의된 값만 허용한다.
- path와 root는 wildcard 규칙을 따라야 한다.

---

## 11. 지원하는 Route Operator

Route Rule 에서 사용할 수 있는 operator를 정의한다.

### 형식
```conf
GET / <- /var/www/html
GET /upload -> /data/upload
GET /list <i- /var/www/list
GET /old =301> /new
```

### 규칙
- `<-` 는 SERVEFROM 이다.
- `->` 는 POINT 이다.
- `<i-` 는 AUTOINDEX 이다.
- `=300>` 는 MULTIPLECHOICES 이다.
- `=301>` 는 REDIRECT 이다.
- `=302>` 는 FOUND 이다.
- `=303>` 는 SEEOTHER 이다.
- `=304>` 는 NOTMODIFIED 이다.
- `=307>` 는 TEMPORARYREDIRECT 이다.
- `=308>` 는 PERMANENTREDIRECT 이다.
- 정의되지 않은 operator는 허용하지 않는다.

---

## 12. Path Pattern 규칙

`PathPattern` 은 route path/root 매칭과 rewrite에 사용된다.

### 형식
```conf
GET /login <- /pages/login.html
GET /download/ <- /data/files/
GET /images/*.png <- /static/*.png
GET /user/*/profile <- /profile/*
```

### 규칙
- `*` wildcard를 지원한다.
- wildcard가 없으면 exact match로 동작한다.
- `/` 로 끝나는 패턴은 prefix match로 동작한다.
- wildcard `*` 는 최소 1글자 이상 매칭해야 한다.
- wildcard `*` 는 파일, directory, sub-directory 등 모든 경로 요소를 매칭할 수 있다.

---

## 13. Wildcard 제한

wildcard 사용에는 제한이 있다.

### 형식
```conf
GET /images/* <- /static/*
GET /user/*/profile <- /profile/*
GET /images/*.(png|jpg|jpeg) <- /static/*
```

### 규칙
- 하나의 path segment 안에서 wildcard * 는 2개 이상 허용하지 않는다.
- path와 root의 wildcard 개수는 호환되어야 한다.
- 확장 패턴 `*.(...)` 는 path 쪽에서 여러 후보로 확장된 뒤, root 쪽의 일반 wildcard `*` 와 매칭되는 방식으로 처리한다.
- path와 root의 wildcard 구조가 맞지 않으면 route 생성이 실패한다.
- AUTOINDEX route의 path는 wildcard 없이 directory 형태(`/download/`)로 작성한다.

---

## 14. 확장 패턴 문법

특정 path segment에서 여러 확장자 후보를 한 번에 선언할 수 있다.

### 형식
```conf
GET /images/*.(png|jpg|jpeg) <- /static/*
GET /assets/*.(css|js) <- /public/*
```

### 규칙
- 확장 패턴은 `*.(a|b|c)` 형식으로 작성한다.
- 괄호 내부의 후보는 `|` 로 구분한다.
- 후보는 최소 2개 이상이어야 한다.
- 각 후보는 영숫자만 허용한다.
- 확장 패턴은 path 쪽 segment에서만 사용한다.
- root 쪽은 확장 패턴으로 함께 맞추지 않고 일반 wildcard(`*`)를 사용한다.
- 내부적으로는 path 쪽 후보마다 별도의 `PathPattern` 으로 확장하여 저장한다.
- 잘못된 괄호 형식, 빈 후보, 영숫자가 아닌 문자가 포함된 후보는 허용하지 않는다.
---

## 15. Route 하위 세부 설정

route 선언 다음 줄에서 추가 설정을 할 수 있다.

### 형식
```conf
GET / <- /var/www/html
  ? index.html
  @ basic_auth
  ->{} 10MB
  ! 404:/errors/notfound.html
```

### 규칙
- 하위 설정은 indent level 2에서 작성한다.
- `?` 는 index file 설정이다.
- `@` 는 auth info 설정이다.
- `->{}` 는 max body size 설정이다.
- `!` 는 route 전용 error page 설정이다.

---

## 16. index file 설정

route의 기본 index 파일을 설정한다.

### 형식
```conf
  ? index.html
```

### 규칙
- `.html`, `.htm` 만 허용한다.

---

## 17. auth info 설정

route의 인증 정보를 설정한다.

### 형식
```conf
  @ basic_auth
```

### 규칙
- 값은 하나만 허용한다.

---

## 18. max body size 설정

route의 최대 body 크기를 설정한다.

### 형식
```conf
  ->{} 10MB
  ->{} 512KB
  ->{} 4MiB
```

### 규칙
- 숫자만 쓰면 KB로 해석한다.
- `KB`, `KiB`, `MB`, `MiB` 단위를 허용한다.
- 잘못된 단위는 허용하지 않는다.

---

## 19. route 전용 error page 설정

route 별 에러 페이지를 설정한다.

### 형식
```conf
  ! 404:/errors/notfound.html
```

### 규칙
- `상태코드:경로` 형식이어야 한다.
- route 별 error page map에 저장된다.

---

## 20. Route CGI 설정

Route 단위 CGI를 선언한다.

### 형식
```conf
GET /cgi-bin/test $/usr/bin/python(SCRIPT_MODE=prod)
  ...1.5
  SCRIPT_NAME=test.py
  ROOT_DIR=/var/www/cgi
```

### 규칙
- 첫 줄은 `METHOD PATH CGI설정` 형식이다.
- 총 3개 토큰이어야 한다.
- METHOD 는 `GET`, `POST`, `DELETE` 만 허용한다.
- 마지막 값은 반드시 `$` 문자로 시작해야 한다.
- `$` 뒤에는 문자열 또는 숫자 문자열이 와야 한다.
- 괄호 `()` 안에는 키=값 형식의 옵션을 작성할 수 있다.
- 괄호 안의 키=값 형식은 최대 1개까지만 허용한다.
- `키=값` 옵션이 필요하지 않은 경우 괄호는 생략할 수 있다.
- 내부 block에서는 timeout과 env 설정이 가능하다.
- 내부 block의 각 줄은 indent level 2여야 한다.
- 내부 block의 줄 끝 공백은 허용하지 않는다.

---

## 21. CGI timeout 규칙

CGI timeout을 설정한다.

### 형식
```conf
...3
...0.5
```

### 규칙
- `...` 으로 시작해야 한다.
- 숫자 또는 소수를 허용한다.
- 범위는 `0.05 < timeout <= 15.0` 이다.

---

## 22. 환경변수 규칙

CGI 환경변수는 `KEY=VALUE` 형식으로 설정한다.

### 형식
```conf
SCRIPT_NAME=test.py
ROOT_DIR=/var/www/cgi
ENV_MODE=prod
```

### 규칙
- `KEY=VALUE` 형식이어야 한다.
- KEY 는 비어 있으면 안 된다.
- 대문자, `_`, 숫자를 허용한다.
- 첫 글자는 숫자가 될 수 없다.
- 중복 key는 허용하지 않는다.

---

## 23. Path Rewrite 규칙

rewrite 시 `PathPattern::rewrite_path()` 를 사용한다.

### 형식
```conf
GET /download/* <- /data/*
```

### 규칙
- from(path rule)에 wildcard가 있으면 wildcard 캡처값을 root에 삽입한다.
- from이 `/` 로 끝나면 suffix를 root 뒤에 이어붙인다.
- wildcard 개수가 맞지 않으면 실패한다.
- root mapping 형태일 경우 relative path 기반 rewrite를 수행할 수 있다.

---

## 24. Route 매칭 규칙

현재 `find_route()` 기준으로 route를 찾는다.

### 형식
```conf
GET /images/* <- /var/www/assets/*
GET /images/logo.png <- /var/www/static/logo.png
GET /download/ <i- /var/www/download/
GET /old =301> /new
```

### 규칙
- method가 일치해야 한다.
- `route.path.matches(request_path)` 가 true인 첫 route를 반환한다.
- 현재 구현에서는 선언 순서가 중요하다.

---

## 25. 참고: AUTOINDEX / REDIRECT 처리 정책

현재 문서화된 정책은 아래와 같다.

### 형식
```conf
GET /download/ <i- /var/www/download/
GET /old =301> /new
```

### 규칙
- AUTOINDEX route의 path는 wildcard 없이 directory 형태(`/download/`)로 작성한다.
- AUTOINDEX는 해당 directory 자신과 그 하위 path들을 대상으로 처리하는 정책을 따른다.
- REDIRECT는 일반 route와 동일한 매칭 규칙을 사용한다.