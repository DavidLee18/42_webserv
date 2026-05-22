# Config File Rule 정리 (최신 코드 기준)

현재 구현된 `WebserverConfig`, `ServerConfig`, `RouteRule_CGI`, `PathPattern`, `utils` 기준으로 config 파일의 작성 규칙을 정리한다.

이 문서는 config 작성자가 지켜야 할 문법을 중심으로 설명하며, 실제 코드의 파싱 흐름과 검증 조건을 기준으로 작성한다.

> 참고: 이 문서는 config 문법과 검증 규칙을 중심으로 정리한다. 파일 시스템 경로를 어떤 기준 디렉토리에서 해석할지는 문법 규칙과 분리하여 별도 정책으로 다룬다.

---

## 1. 전체 구성

config 파일은 아래 순서로 작성한다.

```conf
types =
	...

cgi =
	...

:8080 =
	...
```

### 규칙

- `types =` 또는 `types=` 는 필수이다.
- `types` 블록은 server 블록보다 먼저 작성해야 한다.
- `cgi =` 또는 `cgi=` 는 선택 항목이다.
- `cgi` 블록도 server 블록보다 먼저 작성해야 한다.
- server 블록은 하나 이상 필요하다.
- 최상위에서 정의되지 않은 형식은 오류로 처리된다.
- 최상위 level 0에서 전역 기본 에러 페이지는 파싱하지 않는다.
- 에러 페이지는 route 하위 설정에서만 `! <status>:<path>` 형식으로 작성한다.

---

## 2. 공통 작성 규칙

### 들여쓰기

| 위치 | indent level |
|---|---:|
| 최상위 항목 | 0 |
| `types` / `cgi` 블록 내부 | 1 |
| server 블록 내부 항목 | 1 |
| route / Route CGI 하위 설정 | 2 |
| header continuation line | 2 |

### 규칙

- 들여쓰기는 tab(`\t`) 기준이다.
- 기대 indent level과 실제 tab 개수가 다르면 오류이다.
- level 0 줄 앞에는 공백이나 tab이 있으면 안 된다.
- level 1 이상 줄은 필요한 개수만큼 tab으로 시작해야 한다.
- 줄 끝에 trailing whitespace가 있으면 오류이다.

### 빈 줄 규칙

- 빈 줄은 현재 block을 종료하는 기준으로 사용된다.
- server 블록 내부에서 일반 Route Rule 또는 Route CGI는 반드시 빈 줄 하나로 구분해야 한다.
- Route Rule 또는 Route CGI의 하위 설정은 해당 route 바로 아래에 이어서 작성한다.
- 빈 줄 없이 다음 route를 작성하면 이전 route의 하위 block으로 해석되어 오류가 발생한다.
- server 블록 안에서 빈 줄이 두 번 연속 나오면 server 블록 종료로 처리된다.
- 따라서 server 블록 내부 항목을 구분할 때는 빈 줄을 정확히 하나만 사용한다.

---

## 3. `types` 블록

MIME type 매핑을 정의한다.

### 형식

```conf
types =
	html -> text/html
	css -> text/css
	js -> application/javascript
	png|jpg|jpeg -> image/jpeg
	_ -> application/octet-stream
```

### 규칙

- 각 항목은 `확장자 -> MIME타입` 형식이다.
- 여러 확장자는 `|` 로 연결할 수 있다.
- 같은 확장자를 중복 선언할 수 없다.
- `_` 는 default MIME type을 의미하며 반드시 한 번 정의해야 한다.
- MIME type은 `type/subtype` 형식이어야 한다.

---

## 4. 전역 CGI 블록

CGI 파일 확장자와 실행 파일 경로를 매핑한다.

### 형식

```conf
cgi =
	php -> /usr/bin/php-cgi
	py -> /usr/bin/python3
```

### 규칙

- 각 항목은 `확장자 -> 실행파일경로` 형식이다.
- 확장자에는 `.` 을 쓰지 않는다.
- 확장자와 실행파일경로에는 공백을 포함할 수 없다.
- 같은 확장자를 중복 선언할 수 없다.
- 전역 CGI 블록은 최대 한 번만 정의할 수 있다.
- 전역 CGI에 등록된 확장자는 Route CGI executable 검사에 사용된다.
- 전역 CGI 확장자와 별개로 `.cgi` 확장자는 기본 허용된다.
- Route CGI executable은 `.cgi` 또는 전역 CGI에 등록된 확장자로 끝나야 한다.
- 경로 중간에 같은 확장자 문자열이 있어도 executable 끝 확장자가 아니면 인정하지 않는다.
- 전역 CGI 실행파일 경로는 실행 가능한 regular file이어야 한다.
- 파일 시스템 경로 기준 정책은 문법 규칙과 분리하여 별도로 다룬다.

---

## 5. Server 블록

server 블록은 port 번호를 기준으로 선언한다.

### 형식

```conf
:8080 =
	[] +<= Server: webserv
	...3000

	GET / <- /var/www/html
```

또는 아래 형식도 허용된다.

```conf
:8080=
	GET / <- /var/www/html
```

### 규칙

- server header는 `:PORT =` 또는 `:PORT=` 형식이다.
- port 범위는 `1024 ~ 49151` 이다.
- 같은 port의 server 블록을 중복 선언할 수 없다.
- server 블록은 최소 하나 이상의 일반 Route Rule 또는 Route CGI를 포함해야 한다.
- Header와 Server response time은 Route Rule 또는 Route CGI보다 먼저 작성해야 한다.
- route가 나온 뒤에 Header 또는 Server response time을 작성하면 오류이다.

---

## 6. Header와 Server response time

### Header 형식

```conf
	[] +<= Server: webserv
	[] +<= X-Test: hello;
		world
```

### Header 규칙

- header 시작 줄은 `[] +<= Header-Name: value` 형식이다.
- header line에는 `:` 가 정확히 한 번 있어야 한다.
- header name은 비어 있으면 안 된다.
- 줄 끝이 `;` 로 끝나면 다음 줄을 continuation line으로 읽는다.
- continuation line은 indent level 2여야 한다.

### Server response time 형식

```conf
	...3000
```

### Server response time 규칙

- `...` 뒤에는 unsigned integer 문자열이 와야 한다.
- 허용 범위는 `1 ~ 60000` 이다.
- 단위는 millisecond이다.
- server 블록 안에서 최대 한 번만 정의할 수 있다.


---

## 7. Route Rule

일반 route는 요청 method, 요청 path, operator, 대상 path로 구성된다.

### 형식

```conf
	GET / <- /var/www/html
	GET|POST /upload -> /data/upload
	GET /download/ <i- /var/www/download/
	GET /old =301> /new
```

### 규칙

- 형식은 `METHOD PATH OPERATOR TARGET` 이다.
- 공백 기준 토큰은 정확히 4개여야 한다.
- METHOD는 `GET`, `POST`, `DELETE` 만 허용한다.
- 여러 method는 `|` 로 연결할 수 있다.
- 여러 method를 작성하면 method별 route가 각각 생성된다.
- route는 작성 순서대로 저장된다.
- 요청 처리 시 먼저 매칭되는 route가 선택된다.
- 다음 Route Rule 또는 Route CGI를 이어 작성하려면 이전 route block 뒤에 반드시 빈 줄 하나를 넣어야 한다.

### 지원 operator

| Operator | 의미 |
|---|---|
| `<-` | SERVE_FROM |
| `->` | UPLOAD_TO |
| `<i-` | AUTOINDEX |
| `#` | LOGIN_USING |
| `=300>` | MULTIPLE_CHOICES |
| `=301>` | REDIRECT |
| `=302>` | FOUND |
| `=303>` | SEE_OTHER |
| `=304>` | NOT_MODIFIED |
| `=307>` | TEMPORARY_REDIRECT |
| `=308>` | PERMANENT_REDIRECT |

---

## 8. Route 하위 설정

일반 route 선언 다음 줄에서 route별 추가 설정을 작성할 수 있다.

### 형식

```conf
	GET / <- /var/www/html
		? index.html
		@ auth/basic_auth
		->{} 10MB
		! 404:/errors/notfound.html
```

### 규칙

- 하위 설정은 indent level 2에서 작성한다.
- 하위 설정은 빈 줄 또는 EOF를 만나면 종료된다.
- 하위 설정은 공백 기준 정확히 2개 토큰이어야 한다.

| 키워드 | 의미 | 주요 규칙 |
|---|---|---|
| `?` | index file | 존재하고 읽기 가능한 `.html` 파일 |
| `@` | auth info | 존재하는 파일 경로 |
| `->{}` | max body size | `KB`, `KiB`, `MB`, `MiB` 허용 |
| `!` | route error page | `400 ~ 599` 상태코드와 `.html` 파일 |

### Route error page 규칙

- route error page는 route 하위 설정에서만 작성한다.
- 형식은 `! <status>:<path>` 이다.
- status는 3자리 HTTP error status code여야 한다.
- status는 `400 ~ 599` 범위여야 한다.
- `:` 구분자는 정확히 한 번만 사용해야 한다.
- path는 존재하고 읽기 가능한 regular file이어야 한다.
- path 확장자는 `.html` 이어야 한다.

### Max body size 규칙

- 숫자만 쓰면 KB 단위로 해석한다.
- `KB`, `KiB` 는 값 그대로 KB로 저장된다.
- `MB` 는 `숫자 * 1000` KB로 변환된다.
- `MiB` 는 `숫자 * 1024` KB로 변환된다.
- 최대값은 `1048576KB` 이다.

---

## 9. Route CGI

Route 단위 CGI를 선언한다.

### 형식

```conf
	GET /cgi-bin/test $/cgi-bin/test.cgi(MODE=prod)
		...3000
		SCRIPT_NAME=test.cgi
		ROOT_DIR=/var/www/cgi
```

inline env 없이 작성할 수도 있다.

```conf
	POST /cgi-bin/upload $/cgi-bin/upload.cgi
		...5000
		UPLOAD_MODE=on
```

### 규칙

- 첫 줄은 `METHOD PATH CGI설정` 형식이다.
- 공백 기준 토큰은 정확히 3개여야 한다.
- METHOD는 `GET`, `POST`, `DELETE` 만 허용한다.
- 세 번째 토큰은 반드시 `$` 로 시작해야 한다.
- CGI executable path는 `.cgi` 또는 전역 `cgi =` 블록에 등록된 확장자로 끝나야 한다.
- 경로 중간에 허용 확장자 문자열이 포함되어 있어도, executable path의 끝 확장자가 아니면 허용되지 않는다.
- CGI executable path는 실행 가능한 regular file이어야 한다.
- inline env는 executable path 바로 뒤에 `(KEY=VALUE)` 형식으로 최대 한 개만 작성할 수 있다.
- inline env를 작성하는 경우 `(KEY=VALUE)`는 해당 CGI 설정 문자열의 마지막에 위치해야 하며, `)` 뒤에 다른 문자를 붙일 수 없다.
- Route CGI 하위 block에서는 timeout과 env 설정을 작성할 수 있다.
- 다른 Route Rule 또는 Route CGI를 이어 작성하려면 CGI block 뒤에 반드시 빈 줄 하나를 넣어야 한다.

### CGI executable과 inline env

```conf
	GET /run $/cgi-bin/run.cgi
	GET /run $/cgi-bin/run.cgi(MODE=prod)
```

- `$` 뒤에는 CGI executable path가 온다.
- executable path는 `.cgi` 또는 전역 `cgi =` 블록에 등록된 확장자로 끝나야 한다.
- inline env가 있다면 executable path 바로 뒤에 `(KEY=VALUE)` 형식으로 작성한다.
- inline env는 최대 한 개만 허용된다.
- `(KEY=VALUE)` 뒤에는 다른 문자가 올 수 없다.

### CGI timeout

```conf
		...3000
```

- `...` 뒤에는 unsigned integer 문자열이 와야 한다.
- 허용 범위는 `1 ~ 3600000` 이다.
- 단위는 millisecond이다.
- 소수는 허용하지 않는다.

### CGI 환경변수

```conf
		SCRIPT_NAME=test.cgi
		ROOT_DIR=/var/www/cgi
```

- 형식은 `KEY=VALUE` 이다.
- CGI 환경변수 줄에는 공백을 포함할 수 없다.
- `=` 는 정확히 한 번만 사용해야 한다.
- KEY와 VALUE는 비어 있으면 안 된다.
- KEY에는 대문자, `_`, 숫자를 사용할 수 있다.
- KEY의 첫 글자는 숫자가 될 수 없다.
- 같은 CGI context 안에서 같은 KEY를 중복 선언할 수 없다.


---

## 10. Path Pattern과 Wildcard

### 기본 매칭 규칙

- `*` wildcard를 지원한다.
- wildcard가 있는 pattern은 wildcard match로 처리한다.
- wildcard가 없는 pattern이 `/` 로 끝나면 prefix match로 처리한다.
- wildcard가 없고 `/` 로 끝나지 않으면 exact match로 처리한다.
- `*` 는 최소 1글자 이상과 매칭되어야 한다.
- `*` 는 빈 문자열과 매칭되지 않는다.
- `*` 는 `/` 도 포함해서 매칭할 수 있다.

### 예시

| Pattern | Request | 결과 |
|---|---|---:|
| `/download/` | `/download/a.txt` | match |
| `/download/` | `/download` | no match |
| `/download/*` | `/download/a.txt` | match |
| `/download/*` | `/download/` | no match |
| `/images/*.png` | `/images/a.png` | match |
| `/images/*.png` | `/images/.png` | no match |

### wildcard 제한

- 하나의 path segment 안에서 `*` 는 최대 1개만 사용할 수 있다.
- 왼쪽 PATH에서는 `*.png`, `*.(png|jpg|jpeg)`, `/user/*/profile` 같은 패턴을 사용할 수 있다.
- 오른쪽 TARGET에서 wildcard는 반드시 독립된 `*` segment로만 존재해야 한다.
- 오른쪽 TARGET의 `/static/*` 는 허용된다.
- 오른쪽 TARGET의 `/static/*.png` 는 허용되지 않는다.
- 오른쪽 TARGET의 wildcard segment 수와 왼쪽 PATH의 wildcard 포함 segment 수가 같아야 한다.

---

## 11. 확장 패턴

특정 path segment에서 여러 확장자 후보를 한 번에 선언할 수 있다.

### 형식

```conf
	GET /images/*.(png|jpg|jpeg) <- /static/*
	GET /assets/*.(css|js) <- /public/*
```

### 규칙

- 확장 패턴은 `*.(a|b|c)` 형식이다.
- 괄호 내부 후보는 `|` 로 구분한다.
- 후보는 최소 2개 이상이어야 한다.
- 각 후보는 영문자 또는 숫자로만 구성되어야 한다.
- 확장 패턴은 path 쪽에서 확장된다.
- root/TARGET 쪽은 일반 wildcard `*` segment로 받는다.

### 확장 예시

```conf
	GET /images/*.(png|jpg) <- /static/*
```

위 설정은 내부적으로 아래와 유사하게 확장된다.

```conf
	GET /images/*.png <- /static/*
	GET /images/*.jpg <- /static/*
```

---

## 12. Path Rewrite

실제 파일 경로 또는 대상 경로 계산에는 `PathPattern::rewrite_path()`가 사용된다.

### 기본 분기

`rewrite_path()`는 config 왼쪽 PATH와 실제 request path를 비교해 아래 방식 중 하나로 target path를 계산한다.

| 조건 | 처리 |
|---|---|
| 왼쪽 PATH에 wildcard가 있음 | wildcard rewrite |
| 왼쪽 PATH에 wildcard가 없고 `/` 로 끝남 | prefix route resolve |
| 왼쪽 PATH에 wildcard가 없고 `/` 로 끝나지 않음 | exact path rewrite |

### 예시 1: 확장자 패턴과 하위 경로 보존

```conf
	GET /images/*.png <- /static/*
```

```text
request: /images/logo.png
result:  /static/logo.png
```

```text
request: /images/a/b/c.png
result:  /static/a/b/c.png
```

위 예시에서 `*` 는 단일 파일명만 의미하지 않는다. `*` 는 `/` 를 포함한 하위 경로까지 매칭할 수 있으며, rewrite 결과에서도 해당 relative path가 오른쪽 TARGET의 독립된 `*` segment 위치에 들어간다.

### 예시 2: prefix route resolve

```conf
	GET /docs/ <- /var/www/docs/
```

```text
request: /docs/
result:  /var/www/docs/
```

```text
request: /docs/a/b.html
result:  /var/www/docs/a/b.html
```

prefix route resolve는 request path에서 왼쪽 PATH와 매칭된 prefix 부분을 제거하고, 남은 suffix를 오른쪽 TARGET 뒤에 이어붙인다.

```text
route_pattern  = /docs/
request_path   = /docs/a/b.html
rewrite_target = /var/www/docs/

suffix = a/b.html
result = /var/www/docs/a/b.html
```

### 규칙

- 왼쪽 PATH에 wildcard가 있으면 요청 path에서 추출한 relative path를 오른쪽 TARGET의 `*` 에 삽입한다.
- 오른쪽 TARGET의 wildcard는 단일 `*` segment로만 작성해야 한다.
- 왼쪽 PATH의 `*` 는 `/` 를 포함한 여러 path segment를 매칭할 수 있다.
- 따라서 `/images/*.png` 는 `/images/a/b/c.png` 와 매칭될 수 있다.
- 왼쪽 PATH가 `/` 로 끝나는 prefix pattern이면 요청 path의 suffix를 TARGET 뒤에 이어붙인다.
- 왼쪽 PATH와 요청 path가 exact match이면 TARGET을 그대로 반환한다.
- rewrite 결과는 연속된 `/` 를 하나로 정규화한다.

### 주의: 중간 wildcard 사용

```conf
	GET /user/*/profile <- /profiles/*
```

위와 같은 중간 wildcard 패턴은 순수 capture 치환 방식으로 해석되지 않을 수 있다. 현재 rewrite 정책은 wildcard capture 하나만 치환하는 방식이 아니라, 요청 path의 relative path를 오른쪽 TARGET의 `*` 에 넣는 방식에 가깝다. 따라서 rewrite가 필요한 route에서는 wildcard를 path 끝부분의 파일 또는 하위 경로 매핑 용도로 사용하는 것을 기준으로 한다.


---

## 13. 파일 시스템 경로 해석 정책

이 문서는 config 문법과 검증 규칙을 정리한다. 파일 시스템 경로 기준은 문법 규칙과 분리하여 별도 정책으로 다룬다.

따라서 이 문서에서는 아래 항목을 확정하지 않는다.

- 일반 Route Rule의 TARGET 경로 기준
- `@` auth file 경로 기준
- route error page 경로 기준
- Route CGI executable 경로 기준
- 전역 CGI executable 경로와 일반 route 경로의 기준 차이

현재 문법상 경로는 config에 작성된 문자열을 기준으로 검증되며, 실제 기준 디렉토리 정책은 별도 정책에 따른다.

---

## 14. Route 매칭 정책

### 일반 Route Rule

- method가 일치해야 한다.
- `route.path.matches(request_path)` 가 true인 첫 번째 route를 반환한다.
- longest match를 자동으로 계산하지 않는다.
- 더 구체적인 route를 우선하고 싶다면 config 파일에서 더 위에 작성해야 한다.
- config 작성 순서가 route 우선순위이다.

### Route CGI

- method가 일치해야 한다.
- `RouteRule_CGI.path.matches(request_path)` 가 true인 첫 번째 CGI route를 반환한다.
- Route CGI도 작성 순서가 우선순위이다.

---

## 15. AUTOINDEX와 REDIRECT

### AUTOINDEX

```conf
	GET /download/ <i- /var/www/download/
```

- AUTOINDEX는 `<i-` operator를 사용한다.
- AUTOINDEX path는 wildcard 없이 prefix route 형태로 작성하는 것을 기준으로 한다.
- path가 `/` 로 끝나면 prefix match로 처리된다.
- `/download/` 는 `/download/a.txt`, `/download/dir/a.txt` 와 매칭될 수 있다.
- `/download` 처럼 trailing slash가 없는 요청은 `/download/` 와 매칭되지 않는다.
- AUTOINDEX는 prefix route resolve로 계산된 실제 directory path를 기준으로 directory listing을 생성한다.

### REDIRECT

```conf
	GET /old =301> /new
	GET /temp =302> /temporary
```

- REDIRECT 계열 operator는 `=30x>` 형식을 사용한다.
- REDIRECT도 일반 route와 동일하게 method와 path로 매칭된다.
- 작성 순서가 우선순위이다.
- wildcard 규칙은 일반 route와 동일하게 적용된다.

---

## 16. 전체 예시

아래 예시처럼 server 블록 내부에서 일반 Route Rule과 Route CGI 묶음은 빈 줄 하나로 구분해야 한다.
빈 줄 두 개가 연속되면 server 블록 종료로 해석될 수 있다.

```conf
types =
	html -> text/html
	css -> text/css
	js -> application/javascript
	png|jpg|jpeg -> image/jpeg
	_ -> application/octet-stream

cgi =
	py -> /usr/bin/python3

:8080 =
	[] +<= Server: webserv
	...3000

	GET /private/ <- /var/www/private/
		? index.html
		@ auth/basic_auth
		->{} 10MB
		! 404:/errors/private_404.html

	GET /images/*.(png|jpg|jpeg) <- /var/www/static/images/*

	GET /download/ <i- /var/www/download/

	GET /old =301> /new

	GET /cgi-bin/test $/cgi-bin/test.cgi(MODE=prod)
		...3000
		SCRIPT_NAME=test.cgi
		ROOT_DIR=/var/www/cgi
```

---

## 17. 핵심 주의사항

- Route Rule과 Route CGI는 반드시 빈 줄 하나로 구분한다.
- 빈 줄 두 개는 server block 종료로 처리될 수 있다.
- route 선택은 longest match가 아니라 작성 순서 우선이다.
- 더 구체적인 route를 먼저 작성해야 한다.
- 최상위 level 0에서는 전역 기본 에러 페이지를 파싱하지 않는다.
- route error page는 route 하위 설정에서만 작성한다.
- 오른쪽 TARGET의 wildcard는 반드시 독립된 `*` segment여야 한다.
- `*` 는 빈 문자열과 매칭되지 않고 최소 1글자 이상과 매칭된다.
- `*` 는 `/` 를 포함한 하위 경로까지 매칭할 수 있다.
- rewrite 시 TARGET의 `*` 에는 단일 파일명뿐 아니라 relative path가 들어갈 수 있다.
- wildcard가 없고 `/` 로 끝나는 왼쪽 PATH는 prefix route로 처리된다.
- prefix route는 request path에서 매칭된 prefix를 제거한 suffix를 TARGET 뒤에 붙인다.
- 파일 시스템 경로 기준 정책은 문법 규칙과 분리하여 별도 정책으로 다룬다.
- HTML 파일 검사는 `.html` 만 허용한다.