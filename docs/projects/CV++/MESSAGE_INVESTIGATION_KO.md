# CV++ Message Investigation

## 문서 정보
- 버전: `v0.3`
- 상태: `현재 parser taxonomy 기준으로 갱신됨`
- 작성일: `2026-03-24`
- 최종 수정일: `2026-03-25`
- 작성 주체: `Software Engineer Agent`

## 목적
이 문서는 실제 저장된 세션과 현재 parser 구현을 기준으로 parser의 `message` 값이 무엇을 의미하는지 기록한다. 즉, parser 의도만이 아니라 live stream에서 실제로 어떤 상태를 나타내는지 정리한다.

## 범위
이번 조사 범위:
- `2026-03-24` 이전 저장 세션의 historical label
- 갱신된 parser가 사용하는 현재 label
- fragmented Hanwha payload가 truncation과 continuation 상태를 어떻게 거치는지

## 발견 1: fragmented object delivery는 여전히 기본 기준선이다
저장된 세션 전체 parsed summary 분포:
- `malformed-payload`: `21288`
- `no-objects`: `3492`
- `success`: `4425`

`objects > 0`인 payload만 따로 보면:
- `malformed-payload`: `17721`
- `success`: `4425`

object-bearing payload 비율:
- `malformed-payload`: `80.02%`
- `success`: `19.98%`

해석:
- object-bearing metadata 자체는 흔하지만, clean object-bearing payload는 여전히 소수다.
- 따라서 parser-health message는 Milestone 3에서 여전히 운영상 중요하다.

## 발견 2: 새 taxonomy는 예전 관측 동작을 다시 이름 붙인 것이다
historical-to-current mapping:
- `Object block did not close cleanly`
  - 현재 의도: `truncated-object-fragment`
- empty message on `malformed-payload`
  - 현재 의도: 지금은 `recovered-continuation`으로 이름 붙은 continuation recovery
- `No <tt:Object> blocks found`
  - 현재 의도: `metadata-without-objects`
- `XML start marker not found`
  - 현재 의도: `continuation-without-xml-start`

해석:
- 새 label은 stream behavior와 더 잘 맞는다.
- 그렇다고 이전 세션과 다른 카메라 조건을 뜻하는 것은 아니다.

## 발견 3: `truncated-object-fragment`는 예전 truncation note의 올바른 의미다
관측된 동작:
- 현재 parser는 object block이 시작되었지만 현재 payload 안에서 닫히지 않으면 `truncated-object-fragment`를 낸다.
- 이는 `session-20260323-133443`에서 보였던 예전 `Object block did not close cleanly` 동작과 일치한다.

현재 대표 예시:
- 세션: `session-20260324-163207`
- parsed line:
  - `status=malformed-payload message="truncated-object-fragment" objects=2 id=169384,type=Head,... id=169375,type=Human,...`

해석:
- usable하지만 incomplete한 object payload에 붙이기 적절한 operator-facing 이름이다.

## 발견 4: `recovered-continuation`은 현재 continuation의 기본 label이다
최근 `profile2` 기준 세션 `session-20260324-163207`에서 관측된 값:
- `recovered-continuation`: `2574`
- `truncated-object-fragment`: `2279`
- `clean-object-payload`: `148`
- `metadata-without-objects`: `4`

대표 sequence:
1. `status=malformed-payload message="truncated-object-fragment" objects=2 ...`
2. `status=malformed-payload message="recovered-continuation" objects=5 ...`
3. `status=success message="recovered-continuation" objects=9 ...`

해석:
- continuation recovery는 이제 corner case가 아니라 일반 경로다.
- 같은 continuation chain도 fully clean해지기 전부터 실무적으로 유용할 수 있다.

## 발견 5: `recovered-continuation`은 `malformed-payload`와 `success` 둘 다로 끝날 수 있다
관측된 동작:
- 어떤 continuation chain은 중간 object 상태로 끝나서 `malformed-payload`로 남는다.
- 다른 continuation chain은 eventually clean하게 닫혀 `success`로 끝난다.

해석:
- `message`는 recovery path를 설명한다.
- `status`는 이번 parse가 clean하게 끝났는지, 아니면 여전히 fragmented 상태로 끝났는지를 설명한다.
- 따라서 UI는 둘 중 하나만이 아니라 둘 다 보여주는 것이 맞다.

## 발견 6: `metadata-without-objects`는 parser damage가 아니라 유효한 metadata condition이다
관측된 동작:
- 현재 parser는 `status=no-objects message="metadata-without-objects" objects=0`를 낸다.
- 이전 raw sample과 저장 세션 기준으로도 이것은 motion/state event 같은 event-only metadata와 맞아떨어진다.

해석:
- no-object metadata는 malformed transport case와 분리해서 다뤄야 한다.
- `raw=seen`인데 overlay가 없다고 해서 곧바로 parser failure로 몰면 안 된다.

## 발견 7: parser-noise label은 실제로 존재하지만 드물다
전체 detection 기준:
- 분석한 전체 detection 수: `50856`
- 안정적인 class set 밖 noise-labeled detection 수: `127`
- noise 비율: `0.25%`

가장 흔한 noise label:
- `Cayr`
- `Hukuman`
- `Caar`

해석:
- forensic tool에서는 이런 label도 보존해야 한다.
- 다만 operator metric에서 1급 category로 올려서는 안 된다.

## PM / Tech Lead / SE 해석
PM 관점:
- operator surface는 raw parser internal보다 metadata condition 질문에 답해야 한다.

Tech Lead 관점:
- 새 label은 stream lifecycle state에 더 가깝고, runtime taxonomy의 기준선이 되어야 한다.

SE 관점:
- raw parser note는 log에 남겨야 한다.
- Qt와 이후 SQLite review는 이 위에 더 작은 parser-health breakdown layer를 올리는 방식이 맞다.

## 실무 해석 모델
현재 practical model:
- `success + clean-object-payload`
  - clean object payload
- `success + recovered-continuation`
  - continuation recovery가 clean하게 끝난 경우
- `unknown-pattern + clean-object-payload-with-unknown-patterns`
  - object payload는 있지만 parser coverage가 일부 부족한 경우
- `no-objects + metadata-without-objects`
  - object block이 없는 metadata
- `malformed-payload + truncated-object-fragment`
  - object payload가 중간에서 잘린 경우
- `malformed-payload + continuation-without-xml-start`
  - XML start가 없는 continuation chunk
- `malformed-payload + recovered-continuation`
  - continuation recovery는 유용했지만, 이번 parse는 여전히 fragmented로 끝난 경우

## Message Taxonomy v2
권장 분리 방식:
1. raw parser `status`, `message`는 log와 forensic view에 유지한다.
2. UI와 이후 SQLite review에는 더 작은 operator-facing parser-health category layer를 추가한다.

권장 operator-facing category:
- `clean object payload`
- `recovered continuation`
- `fragmented object payload`
- `continuation chunk`
- `metadata without objects`
- `unknown object pattern`

이 방식이 더 나은 이유:
- 이전 세션의 empty-message ambiguity를 제거한다.
- 현재 parser 구현과 직접 맞는다.
- Hanwha 세션에서 실제로 보이는 stream behavior를 반영한다.

## 권고
현재 parser message set은 forensic layer로 유지하고, 그 위에 operator-facing parser-health view를 올리는 방식으로 가는 것이 맞다.

권장 다음 단계:
- raw `status`, `message` field는 log에 유지
- Qt shell과 이후 SQLite session review에 grouped parser-health count 추가
- 이전 저장 세션과의 비교를 위해 historical old-message interpretation도 문서에 남겨둔다

## 열린 질문
- `recovered-continuation`을 나중에 `recovered-clean`, `recovered-still-fragmented` 같은 operator category로 다시 나눌지
- continuation 여부를 `message`에서 추론하지 말고 parser가 explicit flag로 남겨야 하는지
- parser-noise label을 session metric에서는 `parser-artifact`로 묶을지, 아니면 UI summary에서만 묶을지
