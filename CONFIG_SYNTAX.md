# Config File Rule 정리

현재 구현된 `WebserverConfig`, `ServerConfig`, `RouteRule_CGI`, `PathPattern`, `utils` 기준으로 config 파일 문법과 규칙을 정리한다.

이 문서는 함수명이나 이전 문서 기준이 아니라, 현재 코드의 실제 파싱 흐름과 검증 조건을 기준으로 작성한다.

---

## 1. 최상위 구성

config 파일 최상위에서는 아래 항목을 사용할 수 있다.

```conf
types =
cgi =
! 404:/errors/404.html
:8080 =
```

### 규칙

- `types =` 또는 `types=` 는 필수이다.
- `types` 블록은 server 블록보다 먼저 정의되어야 한다.
- `cgi =` 또는 `cgi=` 는 선택 항목이다.
- `cgi` 블록도 server 블록보다 먼저 정의되어야 한다.
- 전역 기본 에러 페이지 `! <status>:<path>` 는 선택 항목이다.
- 전역 기본 에러 페이지도 server 블록보다 먼저 정의되어야 한다.
- server 블록은 하나 이상 반드시 존재해야 한다.
- 최상위에서 정의되지 않은 형식은 invalid configuration format 으로 처리된다.

---

## 2. 들여쓰기와 공백 규칙

현재 구현에서 들여쓰기는 문법의 일부이다.

| 위치 | indent level |
|---|---:|
| 최상위 항목 | 0 |
| `types` / `cgi` 블록 내부 | 1 |
| server 블록 내부 항목 | 1 |
| route / Route CGI 하위 설정 | 2 |
| header continuation line | 2 |

### 규칙

- 들여쓰기 문자는 tab(`\t`)을 기준으로 검사한다.
- 기대 indent level과 실제 tab 개수가 다르면 오류이다.
- level 0 줄 앞에는 공백이나 tab이 있으면 안 된다.
- level 1 이상 줄은 반드시 tab으로 시작해야 한다.
- 필요한 tab 뒤에 추가 leading space가 있으면 오류이다.
- 줄 끝에 trailing whitespace가 있으면 오류이다.
- 빈 줄 또는 EOF는 현재 블록을 종료하는 기준으로 사용된다.
- server 블록에서는 빈 줄이 연속으로 두 번 나오면 server 블록 종료로 판단한다.

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

- `types =` 또는 `types=` 로 시작한다.
- 각 항목은 `확장자 -> MIME타입` 형식이어야 한다.
- `->` 는 정확히 한 번만 사용해야 한다.
- 여러 확장자는 `|` 로 연결할 수 있다.
- 확장자에는 영문자, 숫자, `_`, `|` 만 사용할 수 있다.
- 같은 확장자는 중복 선언할 수 없다.
- `_` 는 default MIME type을 의미한다.
- `_` 는 반드시 한 번 정의되어야 한다.
- `_` 가 두 번 이상 나오면 오류이다.
- MIME type은 `type/subtype` 형식이어야 한다.
- MIME type에는 영문자, 숫자, `/`, `-` 를 사용할 수 있다.
- MIME type은 `/` 로 시작할 수 없다.
- MIME type의 type 부분은 `-` 로 시작하거나 끝날 수 없다.
- MIME type 안에서 `--` 처럼 연속된 hyphen은 허용하지 않는다.

---

## 4. 전역 CGI 블록

전역 CGI 블록은 CGI 파일 확장자와 실행 파일 경로를 매핑한다.

### 형식

```conf
cgi =
	php -> /usr/bin/php-cgi
	py -> /usr/bin/python3
```

### 규칙

- `cgi =` 또는 `cgi=` 로 시작한다.
- 각 항목은 `확장자 -> 실행파일경로` 형식이어야 한다.
- `->` 는 정확히 한 번만 사용해야 한다.
- 확장자에는 `.` 을 쓰지 않는다.
- 확장자에는 영문자, 숫자, `-`, `_` 만 사용할 수 있다.
- 확장자에 공백이 있으면 오류이다.
- 실행 파일 경로에 공백이 있으면 오류이다.
- 같은 확장자를 중복 선언할 수 없다.
- 전역 CGI 블록은 최대 한 번만 정의할 수 있다.
- 전역 CGI 확장자는 server 내부 Route CGI executable 검사에 사용된다.
- 전역 CGI 확장자와 별개로 `.cgi` 확장자는 기본 허용 목록에 포함된다.

---

## 5. 전역 기본 에러 페이지

전역 기본 에러 페이지는 최상위에서 `!` 로 선언한다.

### 형식

```conf
! 404:/errors/404.html
```

### 규칙

- 형식은 `! <status>:<path>` 이다.
- 공백 기준으로 토큰은 정확히 2개여야 한다.
- `<status>:<path>` 안의 `:` 는 정확히 한 번만 사용해야 한다.
- status는 숫자만 허용한다.
- status는 `400 ~ 599` 범위여야 한다.
- path는 존재하는 regular file이어야 한다.
- path는 읽기 가능해야 한다.
- path 확장자는 `.html` 이어야 한다.
- 전역 기본 에러 페이지 선언은 최대 한 번만 허용된다.

---

## 6. Server 블록

server 블록은 port 번호를 기준으로 선언한다.

### 형식

```conf
:8080 =
	GET / <- /var/www/html
```

또는 아래 형식도 허용된다.

```conf
:8080=
	GET / <- /var/www/html
```

### 규칙

- server header는 `:PORT =` 또는 `:PORT=` 형식이어야 한다.
- `:` 다음에는 숫자가 와야 한다.
- port 범위는 `1024 ~ 49151` 이다.
- 같은 port의 server 블록을 중복 선언할 수 없다.
- server 블록은 최소 하나 이상의 일반 Route Rule 또는 Route CGI를 포함해야 한다.
- server 블록 내부에서 header와 server response time은 route보다 먼저 선언해야 한다.
- route 또는 Route CGI가 나온 뒤에 header나 server response time을 선언하면 오류이다.

---

## 7. Server 블록 내부 허용 항목

server 블록 내부에서는 아래 항목을 사용할 수 있다.

```conf
:8080 =
	[] +<= Server: webserv
	...3000
	GET / <- /var/www/html
	GET /cgi-bin/test $/cgi-bin/test.cgi(MODE=prod)
```

### 규칙

- Header 설정을 사용할 수 있다.
- Server response time 설정을 사용할 수 있다.
- 일반 Route Rule을 사용할 수 있다.
- Route CGI 설정을 사용할 수 있다.
- Header와 Server response time은 Route Rule 또는 Route CGI보다 먼저 작성해야 한다.
- Server response time은 server 블록 안에서 최대 한 번만 정의할 수 있다.
- Header block도 route가 나오기 전에만 정의할 수 있다.

---

## 8. Header 설정

server 응답 header를 추가한다.

### 형식

```conf
	[] +<= Server: webserv
	[] +<= X-Test: hello;
		world
```

### 규칙

- header 시작 줄은 `[] +<= Header-Name: value` 형식이다.
- 첫 토큰은 `[]` 이어야 한다.
- 둘째 토큰은 `+<=` 이어야 한다.
- header line에는 `:` 가 정확히 한 번 있어야 한다.
- header name은 비어 있으면 안 된다.
- header name에는 HTTP token 문자만 허용된다.
- header value는 ASCII printable 문자와 tab만 허용된다.
- header value는 비어 있어도 허용된다.
- 줄 끝이 `;` 로 끝나면 다음 줄을 header value continuation line으로 읽는다.
- continuation line은 indent level 2여야 한다.
- continuation line도 header value 문자 규칙을 따라야 한다.
- 최종 저장 시 value 안의 `;` 는 제거된다.

---

## 9. Server response time

server response time을 millisecond 단위로 설정한다.

### 형식

```conf
	...3000
```

### 규칙

- `...` 으로 시작해야 한다.
- `...` 뒤에는 unsigned integer 문자열이 와야 한다.
- leading zero는 허용하지 않는다. 단, 값 `0` 자체는 숫자 변환 규칙상 표현 가능하지만 범위 검사에서 거부된다.
- 허용 범위는 `1 ~ 60000` 이다.
- 단위는 따로 쓰지 않는다.
- server 블록 안에서 최대 한 번만 정의할 수 있다.

---

## 10. Route Rule 기본 형식

일반 route는 요청 method, 요청 path, operator, 대상 path로 구성된다.

### 형식

```conf
	GET / <- /var/www/html
	GET|POST /upload -> /data/upload
	GET /download/ <i- /var/www/download/
	GET /old =301> /new
```

### 규칙

- 공백 기준 토큰은 정확히 4개여야 한다.
- 형식은 `METHOD PATH OPERATOR TARGET` 이다.
- METHOD는 `GET`, `POST`, `DELETE` 만 허용한다.
- 여러 method는 `|` 로 연결할 수 있다.
- 예: `GET|POST`, `GET|POST|DELETE`
- METHOD가 여러 개이면 method별 route가 각각 생성된다.
- OPERATOR는 지원하는 Route Operator 목록에 있어야 한다.
- PATH와 TARGET은 wildcard 규칙을 따라야 한다.
- route는 작성 순서대로 저장된다.
- 요청 처리 시 먼저 매칭되는 route가 선택된다.

---

## 11. 지원하는 Route Operator

현재 코드에서 지원하는 operator는 아래와 같다.

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

### 규칙

- 정의되지 않은 operator는 허용하지 않는다.
- `=301>` 인 경우 `redirect_target` 에 TARGET이 저장된다.
- 나머지 operator는 `root` 또는 처리 대상 path로 TARGET을 저장한다.

---

## 12. Path Pattern 매칭 규칙

`PathPattern`은 route path와 request path 매칭에 사용된다.

### 형식

```conf
	GET /login <- /pages/login.html
	GET /download/ <- /data/files/
	GET /images/*.png <- /static/*
	GET /user/*/profile <- /profile/*
```

### 규칙

- `*` wildcard를 지원한다.
- wildcard가 있는 pattern은 wildcard match로 처리한다.
- wildcard가 없는 pattern이 `/` 로 끝나면 prefix match로 처리한다.
- wildcard가 없고 `/` 로 끝나지 않으면 exact match로 처리한다.
- `*` 는 최소 1글자 이상과 매칭되어야 한다.
- `*` 는 빈 문자열과 매칭되지 않는다.
- `*` 는 `/` 도 포함해서 매칭할 수 있으므로 파일, directory, sub-directory 경로까지 포함할 수 있다.

### 예시

| Pattern | Request | 결과 |
|---|---|---:|
| `/download/` | `/download/a.txt` | match |
| `/download/` | `/download/dir/a.txt` | match |
| `/download/` | `/download` | no match |
| `/download/*` | `/download/a.txt` | match |
| `/download/*` | `/download/` | no match |
| `/images/*.png` | `/images/a.png` | match |
| `/images/*.png` | `/images/.png` | no match |

---

## 13. Wildcard 사용 제한

wildcard 사용에는 명확한 제한이 있다.

### 형식

```conf
	GET /images/* <- /static/*
	GET /images/*.png <- /static/*
	GET /user/*/profile <- /profile/*
```

### 규칙

- 하나의 path segment 안에서 `*` 는 최대 1개만 사용할 수 있다.
- 왼쪽 PATH의 wildcard 개수와 오른쪽 TARGET의 wildcard 개수는 호환되어야 한다.
- 왼쪽 PATH에서는 `*.png`, `*.(png|jpg|jpeg)`, `/user/*/profile` 같은 패턴을 사용할 수 있다.
- 오른쪽 TARGET에서 wildcard는 반드시 독립된 `*` segment로만 존재해야 한다.
- 오른쪽 TARGET의 `/static/*` 는 허용된다.
- 오른쪽 TARGET의 `/static/*.png` 는 허용되지 않는다.
- 오른쪽 TARGET의 wildcard segment 수와 왼쪽 PATH의 wildcard 포함 segment 수가 같아야 한다.

### 올바른 예

```conf
	GET /images/*.png <- /static/*
	GET /user/*/profile <- /profiles/*
```

### 잘못된 예

```conf
	GET /images/*.png <- /static/*.png
	GET /user/*/profile <- /profiles/*/index/*
```

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
- `*.` 다음에는 반드시 `(` 가 와야 한다.
- segment 마지막 문자는 반드시 `)` 이어야 한다.
- 괄호 내부 후보는 `|` 로 구분한다.
- 후보는 최소 2개 이상이어야 한다.
- 각 후보는 영문자 또는 숫자로만 구성되어야 한다.
- 빈 후보는 허용하지 않는다.
- 확장 패턴은 path 쪽에서 확장된다.
- root/TARGET 쪽은 일반 wildcard `*` segment로 받는다.
- 내부적으로 후보마다 별도의 `PathPattern` 으로 확장되어 route가 여러 개 생성된다.

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

## 15. Route 하위 세부 설정

일반 route 선언 다음 줄에서 추가 설정을 작성할 수 있다.

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
- 지원 키워드는 `?`, `@`, `->{}`, `!` 이다.
- route가 여러 method로 확장된 경우 하위 설정은 해당 method별 route에 적용된다.
- 같은 method와 같은 path를 가진 기존 route가 있으면 해당 route에 하위 설정을 적용한다.

---

## 16. Index file 설정

route의 index file을 설정한다.

### 형식

```conf
		? index.html
```

### 규칙

- 키워드는 `?` 이다.
- 값은 존재하는 regular file이어야 한다.
- 값은 읽기 가능해야 한다.
- 값의 확장자는 `.html` 이어야 한다.
- 현재 구현 기준 `.htm` 은 허용되지 않는다.

---

## 17. Auth info 설정

route의 인증 정보 파일을 설정한다.

### 형식

```conf
		@ auth/basic_auth
```

### 규칙

- 키워드는 `@` 이다.
- 값은 하나만 허용한다.
- 값은 존재하는 파일 경로여야 한다.
- 파일이 존재하지 않으면 오류이다.

---

## 18. Max body size 설정

route의 최대 request body 크기를 설정한다.

### 형식

```conf
		->{} 10MB
		->{} 512KB
		->{} 4MiB
		->{} 100
```

### 규칙

- 키워드는 `->{}` 이다.
- 값은 `<숫자><선택 단위>` 형식이다.
- 숫자만 쓰면 KB 단위로 해석한다.
- 허용 단위는 `KB`, `KiB`, `MB`, `MiB` 이다.
- `KB` 와 `KiB` 는 값 그대로 KB로 저장된다.
- `MB` 는 `숫자 * 1000` KB로 변환된다.
- `MiB` 는 `숫자 * 1024` KB로 변환된다.
- leading zero는 허용하지 않는다. 단, `0` 자체는 허용된다.
- 최대값은 `1048576KB` 이다.
- `1024MiB` 를 초과하면 오류이다.

---

## 19. Route 전용 error page 설정

route별 error page를 설정한다.

### 형식

```conf
		! 404:/errors/notfound.html
```

### 규칙

- 키워드는 `!` 이다.
- 형식은 `! <status>:<path>` 이다.
- status는 `400 ~ 599` 범위여야 한다.
- path는 존재하는 regular file이어야 한다.
- path는 읽기 가능해야 한다.
- path 확장자는 `.html` 이어야 한다.
- 설정된 값은 해당 route의 `error_pages` map에 저장된다.

---

## 20. Route CGI 설정

Route 단위 CGI를 선언한다.

### 형식

```conf
	GET /cgi-bin/test $/cgi-bin/test.cgi(MODE=prod)
		...3000
		SCRIPT_NAME=test.cgi
		ROOT_DIR=/var/www/cgi
```

또는 inline env 없이 작성할 수 있다.

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
- `$` 뒤에는 CGI executable path가 와야 한다.
- CGI executable path는 허용된 확장자를 포함해야 한다.
- 허용 확장자는 전역 `cgi =` 블록에서 정의한 확장자와 기본 `.cgi` 이다.
- inline env는 executable 뒤에 `(KEY=VALUE)` 형식으로 최대 1개만 작성할 수 있다.
- inline env가 없다면 괄호는 생략할 수 있다.
- Route CGI 하위 block에서는 timeout과 env 설정을 작성할 수 있다.
- 하위 block의 각 줄은 indent level 2여야 한다.
- 하위 block의 줄에는 공백이 포함되면 안 된다.

---

## 21. Route CGI executable 문법

CGI executable은 `$` 뒤에 작성한다.

### 형식

```conf
	GET /run $/cgi-bin/run.cgi
	GET /run $/cgi-bin/run.cgi(MODE=prod)
```

### 규칙

- executable path는 `.cgi` 또는 전역 `cgi =` 블록에 등록된 확장자를 포함해야 한다.
- executable path 뒤에 추가 문자가 있다면 반드시 `(` 로 inline env가 시작되어야 한다.
- inline env는 `(KEY=VALUE)` 형식이어야 한다.
- inline env 안에는 `=` 가 정확히 한 번만 있어야 한다.
- inline env의 key와 value는 비어 있으면 안 된다.
- inline env는 최대 한 개만 허용된다.
- `)` 뒤에 trailing 문자가 있으면 오류이다.
- executable은 존재하는 regular file이어야 한다.
- executable은 실행 권한이 있어야 한다.

---

## 22. CGI timeout 규칙

Route CGI의 timeout을 millisecond 단위로 설정한다.

### 형식

```conf
		...3000
		...1
		...3600000
```

### 규칙

- `...` 으로 시작해야 한다.
- `...` 뒤에는 unsigned integer 문자열이 와야 한다.
- 소수는 허용하지 않는다.
- leading zero는 허용하지 않는다. 단, 값 `0` 자체는 숫자 변환 규칙상 표현 가능하지만 범위 검사에서 거부된다.
- 허용 범위는 `1 ~ 3600000` 이다.
- 단위는 따로 쓰지 않는다.
- 같은 CGI block 안에서 timeout을 여러 번 작성하면 나중 값으로 덮어쓴다.

---

## 23. CGI 환경변수 규칙

CGI 환경변수는 `KEY=VALUE` 형식으로 작성한다.

### 형식

```conf
		SCRIPT_NAME=test.cgi
		ROOT_DIR=/var/www/cgi
		ENV_MODE=prod
```

### 규칙

- 형식은 `KEY=VALUE` 이다.
- `=` 는 정확히 한 번만 사용해야 한다.
- KEY는 비어 있으면 안 된다.
- VALUE도 비어 있으면 안 된다.
- KEY에는 대문자, `_`, 숫자를 사용할 수 있다.
- KEY의 첫 글자는 숫자가 될 수 없다.
- KEY에는 소문자를 사용할 수 없다.
- 같은 CGI context 안에서 같은 KEY를 중복 선언할 수 없다.
- Route CGI 하위 env와 inline env는 같은 env map에 저장되므로 같은 KEY 중복은 오류이다.

---

## 24. Path Rewrite 규칙

실제 파일 경로 또는 대상 경로 계산에는 `PathPattern::rewrite_path()`가 사용된다.

### 형식

```conf
	GET /download/* <- /data/*
	GET /images/*.png <- /static/*
	GET /docs/ <- /var/www/docs/
```

### 규칙

- 왼쪽 PATH에 wildcard가 있으면 요청 path에서 상대 경로 또는 wildcard 값을 추출해 오른쪽 TARGET의 `*` 에 삽입한다.
- 오른쪽 TARGET의 wildcard는 단일 `*` segment로만 작성해야 한다.
- 왼쪽 PATH가 `/` 로 끝나는 prefix pattern이면 요청 path의 suffix를 TARGET 뒤에 이어붙인다.
- 왼쪽 PATH와 요청 path가 exact match이면 TARGET을 그대로 반환한다.
- rewrite 결과는 연속된 `/` 를 하나로 정규화한다.
- rewrite에 실패하면 빈 문자열을 반환한다.

### 예시

```conf
	GET /images/*.png <- /static/*
```

```text
request: /images/logo.png
result:  /static/logo.png
```

```conf
	GET /docs/ <- /var/www/docs/
```

```text
request: /docs/a/b.html
result:  /var/www/docs/a/b.html
```

---

## 25. Route 매칭 규칙

요청 path와 method에 맞는 route는 `find_route()` 기준으로 선택된다.

### 규칙

- method가 일치해야 한다.
- 단, 요청 method가 `HEAD` 이고 route method가 `GET` 이면 일반 route에서 매칭된다.
- `route.path.matches(request_path)` 가 true인 첫 번째 route를 반환한다.
- longest match를 자동으로 계산하지 않는다.
- 더 구체적인 route를 우선하고 싶다면 config 파일에서 더 위에 작성해야 한다.
- config 작성 순서가 route 우선순위이다.

### 예시

```conf
	GET /download/private/ <- /private
	GET /download/ <- /public
```

위 순서에서는 `/download/private/a.txt` 요청이 `/download/private/` 에 먼저 매칭된다.

```conf
	GET /download/ <- /public
	GET /download/private/ <- /private
```

위 순서에서는 `/download/private/a.txt` 요청도 `/download/` 에 먼저 매칭될 수 있다.

---

## 26. Route CGI 매칭 규칙

Route CGI도 작성 순서대로 검사된다.

### 규칙

- method가 정확히 일치해야 한다.
- 일반 route와 달리 `HEAD` 를 `GET` 으로 대체 매칭하지 않는다.
- `RouteRule_CGI.path.matches(request_path)` 가 true인 첫 번째 CGI route를 반환한다.
- longest match를 자동으로 계산하지 않는다.
- 더 구체적인 CGI route를 우선하고 싶다면 config 파일에서 더 위에 작성해야 한다.

---

## 27. AUTOINDEX 작성 정책

AUTOINDEX는 `<i-` operator를 사용한다.

### 형식

```conf
	GET /download/ <i- /var/www/download/
```

### 규칙

- AUTOINDEX path는 wildcard 없이 directory prefix 형태로 작성하는 것을 기준으로 한다.
- path가 `/` 로 끝나면 prefix match로 처리된다.
- 따라서 `/download/` 는 `/download/a.txt`, `/download/dir/a.txt` 와 매칭될 수 있다.
- `/download` 처럼 trailing slash가 없는 요청은 `/download/` 와 매칭되지 않는다.
- AUTOINDEX도 일반 route와 동일하게 작성 순서 우선 규칙을 따른다.

---

## 28. REDIRECT 작성 정책

REDIRECT 계열 operator는 `=30x>` 형식을 사용한다.

### 형식

```conf
	GET /old =301> /new
	GET /temp =302> /temporary
```

### 규칙

- REDIRECT도 일반 route와 동일하게 method와 path로 매칭된다.
- `=301>` 인 경우 TARGET은 `redirect_target` 에 저장된다.
- 작성 순서가 우선순위이다.
- wildcard 규칙은 일반 route와 동일하게 적용된다.

---

## 29. 현재 문법에서 특히 주의할 점

### 작성 순서 우선

현재 route 선택은 longest match가 아니다.

```text
먼저 작성된 route가 먼저 검사된다.
```

따라서 구체적인 route를 먼저 작성해야 한다.

### 오른쪽 wildcard 제한

오른쪽 TARGET의 wildcard는 반드시 독립된 `*` segment여야 한다.

```conf
# 가능
	GET /images/*.png <- /static/*

# 불가능
	GET /images/*.png <- /static/*.png
```

### wildcard 최소 1글자 규칙

`*` 는 빈 문자열과 매칭되지 않는다.

```text
/download/*  matches     /download/a
/download/*  not matches /download/
```

### `.htm` 주의

현재 `check_html_file()` 기준으로 HTML 파일 검사는 `.html` 만 허용한다.

```conf
# 가능
		? index.html

# 불가능
		? index.htm
```

---

## 30. 간단한 전체 예시

```conf
types =
	html -> text/html
	css -> text/css
	js -> application/javascript
	png|jpg|jpeg -> image/jpeg
	_ -> application/octet-stream

cgi =
	py -> /usr/bin/python3

! 404:/errors/404.html

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
