#ifndef PARSINGUTILS_HPP
#define PARSINGUTILS_HPP

#include "server/Client.hpp"
#include <vector>

namespace utils {
/**
 * @brief 문자열 내에 공백이 존재하는지 확인하는 함수
 * @param line 검사할 문자열
 * @return 공백이 존재하면 true, 그렇지 않으면 false
 */
bool has_space(const std::string &line);
/**
 * @brief 문자열 양쪽 끝의 공백 문자를 제거하는 함수
 * @param s 공백을 제거할 문자열
 * @return 양쪽 끝의 공백이 제거된 문자열
 */
std::string trim_whitespace(const std::string &s);
/**
 * @brief 문자열 앞부분의 들여쓰기 수준이 지정한 값과 일치하는지 확인하는 함수
 * @param line 검사할 문자열
 * @param num 기대하는 들여쓰기 수준
 * @return 들여쓰기 수준이 num과 같으면 true, 그렇지 않으면 false
 *
 * - 탭 1개 또는 공백 4개를 동일한 들여쓰기 1단계로 처리한다.
 */
bool match_indent_level(std::string line, size_t num);
/**
 * @brief 문자열을 구분자를 기준으로 분리하는 함수
 * @param line 분리할 문자열
 * @param delim 문자열을 나눌 기준 구분자
 * @return 구분자를 기준으로 분리된 문자열들을 저장한 벡터
 *
 * - 빈 문자열은 결과에 포함하지 않는다.
 */
std::vector<std::string> string_split(const std::string &line,
                                      const std::string &delim);
/**
 * @brief 문자열에 허용되지 않은 문자가 포함되어 있는지 확인하는 함수
 * @param line 검사할 문자열
 * @param allowed 추가로 허용할 문자 집합
 * @return 허용되지 않은 문자가 존재하면 true, 그렇지 않으면 false
 */
bool has_invalid_char(const std::string &line, const std::string &chars);
/**
 * @brief 문자열에서 특정 문자 또는 문자열의 출현 횟수를 계산한다.
 * @param line 검사할 문자열
 * @param delim 개수를 셀 문자 또는 문자열
 * @return delim의 출현 횟수
 */
int count_occurrences(const std::string &line, const std::string &delim);
/**
 * @brief 문자열 내의 특정 문자를 모두 제거한다.
 * @param s 검사할 문자열
 * @param ch 제거할 문자
 * @return ch가 제거된 문자열
 */
std::string remove_char(std::string s, char ch);
} // namespace utils
#endif
