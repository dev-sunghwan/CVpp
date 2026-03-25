# CV++ Hanwha 메타데이터 기준선 분석

## 문서 정보
- 버전: `v0.1`
- 상태: `초안`
- 작성일: `2026-03-25`
- 최종 수정일: `2026-03-25`
- 작성 주체: `Software Engineer Agent`

## 목적
이 문서는 저장된 CV++ 세션을 바탕으로 현재 Hanwha 메타데이터 기준선을 정리해서, Milestone 3와 Milestone 8을 확장하기 전에 실제 카메라 동작과 parser artifact를 구분할 수 있게 하는 것을 목표로 한다.

## 데이터셋 및 방법
- 데이터셋: `output/session-*` 아래 저장된 `parsed_summary.log`
- 분석 대상 세션 기간: `2026-03-18` ~ `2026-03-24`
- 분석한 전체 parsed payload 수: `29205`
- 분석한 전체 detected object 수: `50856`
- 현재 taxonomy 기준 대표 세션: `output/session-20260324-163207/` (`profile2`)
- 이전 object-heavy 대표 세션: `output/session-20260323-133443/` (`profile4`)

중요한 주의점:
- parser message taxonomy는 `2026-03-24`에 바뀌었다
- 이전 세션은 `Object block did not close cleanly`, empty-message `malformed-payload` 같은 chunk-local note를 사용한다
- 최신 기준 세션은 `truncated-object-fragment`, `recovered-continuation`, `metadata-without-objects` 같은 stream-aware label을 사용한다

## 전체 기준선
저장된 parsed summary 전체 기준 분포:
- `malformed-payload`: `21288` (`72.89%`)
- `no-objects`: `3492` (`11.96%`)
- `success`: `4425` (`15.15%`)

object-bearing payload만 따로 보면:
- 전체 object-bearing payload 수: `22146`
- `malformed-payload`: `17721` (`80.02%`)
- `success`: `4425` (`19.98%`)

해석:
- fragmented transport가 여전히 live 조건의 기본 상태다
- object-bearing metadata가 clean XML delivery와 같다고 보면 안 된다
- operator surface는 overlay 상태와 함께 parser-health 문맥도 보여줘야 한다

## 안정적인 클래스 기준선
전체 저장 세션에서 가장 많이 나온 normalized object type:
- `Car`: `29859`
- `Human`: `9519`
- `Vehicle`: `8215`
- `Bicycle`: `1694`
- `Head`: `632`
- `Bus`: `536`
- `Truck`: `112`
- `Unknown`: `85`
- `Motorcycle`: `77`

전체 저장 세션에서의 unique object ID 기준선:
- `Car`: `353`
- `Vehicle`: `228`
- `Human`: `75`
- `Unknown`: `61`
- `Bicycle`: `31`
- `Bus`: `21`
- `Head`: `12`
- `Truck`: `9`
- `Motorcycle`: `7`

해석:
- `Car`, `Human`, `Vehicle`가 가장 안정적인 주 클래스다
- `Vehicle`과 `Car`는 실제 세션에서 모두 나타나므로 자동으로 같은 클래스로 합치면 안 된다
- `ObjectId`는 한 세션 안에서 반복 detection과 unique tracked object를 구분하는 데 충분히 유용하다

## Parser-noise 기준선
안정적인 normalized class 집합 밖의 label 수:
- 전체 noise-labeled detection 수: `127`
- 전체 detection 대비 noise 비율: `0.25%`

가장 자주 나온 noise label:
- `Cayr`: `16`
- `Hukuman`: `10`
- `Caar`: `8`
- `Hkauuman`: `6`
- `Caur`: `6`

해석:
- parser-noise label은 무시할 정도로 0은 아니지만, 안정적인 normalized label에 비하면 매우 드물다
- session metrics와 이후 SQLite review는 raw evidence를 유지하되, 이런 label을 1급 제품 카테고리처럼 다루지는 말아야 한다

## 현재 Taxonomy 기준선
최근 대표 세션: `output/session-20260324-163207/` (`profile2`)

payload 수:
- 전체 payload 수: `5005`
- `malformed-payload`: `2574` (`51.43%`)
- `success`: `2427` (`48.49%`)
- `no-objects`: `4` (`0.08%`)

object-bearing payload 기준:
- `malformed-payload`: `2574` (`51.47%`)
- `success`: `2427` (`48.53%`)

message 수:
- `recovered-continuation`: `2574`
- `truncated-object-fragment`: `2279`
- `clean-object-payload`: `148`
- `metadata-without-objects`: `4`

대표적인 live sequence:
- `status=malformed-payload message="truncated-object-fragment" objects=2 ...`
- `status=malformed-payload message="recovered-continuation" objects=5 ...`
- `status=success message="recovered-continuation" objects=9 ...`

해석:
- continuation handling은 이제 예외 경로가 아니라 기본 기준선의 일부다
- `recovered-continuation`은 `malformed-payload`와 `success` 둘 다로 끝날 수 있다
- payload가 완전히 clean해지기 전에도 parser는 실무적으로 충분히 유용할 수 있다

## 과거 세션과의 비교 기준선
이전 대표 세션: `output/session-20260323-133443/` (`profile4`)

payload 수:
- `malformed-payload`: `3152`
- `no-objects`: `542`
- `success`: `0`

이전 message note 수:
- `Object block did not close cleanly`: `1647`
- empty message on `malformed-payload`: `1505`
- `No <tt:Object> blocks found`: `542`

해석:
- 새 taxonomy가 새로운 stream condition을 만들어낸 것은 아니다
- 이전 세션에서도 보였던 fragmentation/continuation 동작을 더 명확하게 다시 이름 붙인 것이다

## 실무 기준선 결론
- Hanwha metadata는 `profile2`, `profile4` 모두에서 실제로 존재하고 object-bearing이다
- fragmented delivery는 기본 운영 조건이므로 UI와 storage가 이를 직접 모델링해야 한다
- `ObjectId`는 unique-object metric과 이후 review workflow에 이미 충분히 유용하다
- parser-noise label은 forensic view에서는 유지하되, operator summary에서는 묶거나 비중을 낮춰야 한다
- raw evidence의 source of truth는 여전히 plain log다

## 마일스톤 영향
Milestone 3는 다음 방향으로 이어져야 한다:
- Qt shell과 session summary에 parser-health breakdown을 명시적으로 추가
- clean payload, recovered continuation, fragmented object payload, metadata-without-objects를 구분해서 표시
- headline metric에서는 parser-noise label을 억제하거나 별도 그룹으로 처리

Milestone 8은 다음 방향으로 이어져야 한다:
- session summary, parsed payload status, object detection을 보존하는 SQLite table 설계
- raw XML은 SQLite로 전부 옮기지 않고 `metadata_raw.xml.log`에 유지

## 참고 문서
- `output/session-20260324-163207/parsed_summary.log`
- `output/session-20260324-163207/session.log`
- `output/session-20260323-133443/parsed_summary.log`
- `output/session-20260323-133443/session.log`
- `docs/projects/CV++/METADATA_REFERENCE.md`
- `docs/projects/CV++/MESSAGE_INVESTIGATION.md`
