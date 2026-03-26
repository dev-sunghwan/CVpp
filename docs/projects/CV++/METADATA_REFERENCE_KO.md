# CV++ 메타데이터 레퍼런스

## 문서 정보
- 버전: `v0.3`
- 상태: `현재 parser taxonomy 기준으로 갱신됨`
- 작성일: `2026-03-24`
- 최종 수정일: `2026-03-25`
- 작성 주체: `Software Engineer Agent`

## 목적
이 문서는 CV++가 현재 표시하는 parsed metadata 값이 무엇을 의미하는지 정리해서, SungHwan이 런타임 화면, recent metadata, 세션 로그를 일관된 기준으로 해석할 수 있게 하는 것을 목표로 한다.

## 한눈에 보는 요약 표
| 계층 | 무엇을 뜻하는가 | 현재 필드 / 값 | 예시 | 읽는 방법 |
| --- | --- | --- | --- | --- |
| 카메라 raw metadata | payload 안에 video analytics object data가 있는지 | `<tt:VideoAnalytics><tt:Frame>` 및 `<tt:Object ...>` block이 포함된 raw XML | object-bearing XML payload | 카메라가 event traffic만이 아니라 object metadata도 보냈다는 뜻이다. |
| 카메라 raw metadata | 카메라가 부여한 tracked object ID | `ObjectId` | `169173`, `131951` | 같은 ID가 반복되면 같은 camera-side track의 반복 detection으로 본다. |
| 카메라 raw metadata | normalization 전 카메라 class label | metadata 안의 raw type text | `Car`, `Human`, `Vehicle`, `Bus` | 카메라가 해당 객체를 어떻게 분류하려 했는지 보여준다. |
| 카메라 raw metadata | 카메라가 준 likelihood / confidence | raw likelihood 값 | metadata 안의 `0.82`, 이후 표시되는 `82%` | 앱이 만든 confidence가 아니라 camera-side score다. |
| 파서 출력 | 세션 관점 parse 분류 | `status` | `success`, `malformed-payload`, `no-objects`, `unknown-pattern` | 이번 payload가 clean하게 끝났는지, fragmented로 남았는지, object가 없었는지, parser coverage gap이 있었는지를 보여준다. |
| 파서 출력 | fragment / recovery 설명 | `message` | `clean-object-payload`, `truncated-object-fragment`, `recovered-continuation`, `metadata-without-objects` | parser가 현재 어떤 payload condition을 보고 있다고 판단하는지 설명한다. |
| 파서 출력 | 정규화된 object identifier | `id` | `169173` | 보통 camera `ObjectId`와 직접 대응된다. |
| 파서 출력 | 정규화된 object class | `type` | `Car`, `Human`, `Vehicle`, `Bicycle`, `Unknown` | overlay, metrics, review UI에서 사용하는 parser-normalized class다. |
| 파서 출력 | operator-facing confidence 표시값 | `score` | `85%` | summary와 overlay에서 쓰는 camera likelihood의 percent 형태다. |
| 런타임 / UI | compact operator summary | recent metadata summary line | `success | recovered-continuation | objects=9 | Car #169173 (85%)` | raw XML을 열지 않고도 현재 상황을 가장 빠르게 읽는 줄이다. |

## 빠른 상황별 예시
| 상황 | 카메라 raw 측면 | 파서 측면 | 실무적 의미 |
| --- | --- | --- | --- |
| object metadata가 clean하게 도착 | object-bearing XML payload | `status=success`, `message=clean-object-payload` | 가장 이상적인 object payload 상태이며, overlay와 metrics 근거로 강하다. |
| object metadata가 fragment되었지만 유용함 | fragmented XML / continuation chain | `status=malformed-payload`, `message=recovered-continuation`, `objects>0` | payload가 완전히 clean하지 않아도 metadata는 실제로 존재했고 실무적으로 유효했다는 뜻이다. |
| object 없는 metadata 도착 | event-only 또는 non-object metadata | `status=no-objects`, `message=metadata-without-objects` | metadata traffic은 있었지만 overlay할 object block은 없었다는 뜻이다. |
| object block이 중간에서 끊김 | truncated object fragment | `status=malformed-payload`, `message=truncated-object-fragment` | transport fragmentation이 object payload를 block 중간에서 끊었다는 뜻이다. |
## Parsing Status 값
### `success`
의미:
- payload가 충분히 clean한 상태로 끝나서 parser가 성공적인 object-bearing parse로 판단했다는 뜻이다.

일반적 해석:
- 카메라가 사용 가능한 object metadata를 보냈다는 강한 근거다.
- 이전에 fragment되었던 payload가 완성되면 `message="recovered-continuation"`과 함께 나타날 수도 있다.

### `unknown-pattern`
의미:
- parser가 object block은 찾았지만, 적어도 하나의 object detail pattern이 기대한 형태와 clean하게 맞지 않았다는 뜻이다.

일반적 해석:
- object metadata는 존재한다.
- payload는 유용하지만, 일부 object detail에 대해서는 parser coverage가 불완전하다.

### `no-objects`
의미:
- payload는 metadata였지만 `<tt:Object>` block이 없었다는 뜻이다.

일반적 해석:
- 보통 event-only 또는 non-object metadata다.
- overlay object가 없더라도 metadata traffic이 있었다는 근거로 유용하다.

### `malformed-payload`
의미:
- metadata는 도착했지만, 현재 payload 또는 continuation chain이 아직 fragmented/incomplete 상태라는 뜻이다.

일반적 해석:
- metadata가 없었다는 뜻은 아니다.
- object가 부분적으로 또는 상당 부분 복구될 수 있다.
- 저장된 Hanwha 세션 기준으로도 이는 드문 예외가 아니라 일반적인 live 조건이다.

## 현재 관측되는 Message 값
### `clean-object-payload`
의미:
- 현재 chunk 안에서 object metadata가 clean하게 닫혔다는 뜻이다.

해석:
- 가장 강한 clean baseline이다.
- parser-health metric에 유용하다.

### `clean-object-payload-with-unknown-patterns`
의미:
- payload는 clean하게 닫혔지만, 일부 object-detail pattern은 부분적으로만 이해되었다는 뜻이다.

해석:
- object-bearing payload로서 여전히 유용하다.
- transport loss라기보다 parser coverage gap에 가깝다.

### `metadata-without-objects`
의미:
- metadata는 있었지만 `<tt:Object>` block은 없었다는 뜻이다.

해석:
- 보통 event-only metadata다.
- parser damage와 같은 범주로 묶으면 안 된다.

### `truncated-object-fragment`
의미:
- object block이 시작되었지만 payload가 object block이 닫히기 전에 끝났다는 뜻이다.

해석:
- transport fragmentation이 object payload를 잘랐다는 직접적인 신호다.
- 이후 continuation parse가 뒤따르는 경우가 많다.

### `continuation-without-xml-start`
의미:
- 현재 chunk 안에 `<?xml`이 없어서, fresh XML start가 아니라 continuation fragment처럼 보인다는 뜻이다.

해석:
- transport fragmentation을 보여주는 유용한 신호다.
- parser가 mid-stream continuation을 보고 있다는 뜻인 경우가 많다.

### `recovered-continuation`
의미:
- parser가 continuation chain을 처리하면서 usable object data를 복구했다는 뜻이다.

해석:
- `malformed-payload`와 `success` 둘 다와 함께 나타날 수 있다.
- continuation chain이 여전히 중간에서 끝나면 status는 `malformed-payload`로 남는다.
- continuation chain이 완성되면 status는 `success`가 될 수 있다.

### `recovered-continuation-with-unknown-patterns`
의미:
- continuation recovery는 object를 만들 정도로 성공했지만, 일부 object-detail pattern은 여전히 기대한 parser form과 맞지 않았다는 뜻이다.

해석:
- transport recovery는 성공했다.
- 일부 detail path에 대해서는 parser coverage 개선이 더 필요하다.

## Object Field
### `id`
의미:
- camera-reported object identifier (`ObjectId`)

해석:
- 같은 화면상의 반복 detection을 하나의 tracked object로 보는지 확인할 때 쓴다.
- 같은 `id`의 반복 출현은 새로운 unique object가 아니라 반복 detection event다.

### `type`
의미:
- CV++ normalization 이후의 object class다.

현재까지 가장 안정적으로 관측된 값:
- `Car`
- `Human`
- `Vehicle`
- `Bicycle`
- `Head`
- `Bus`
- `Truck`
- `Motorcycle`
- `Unknown`

메모:
- `Car`와 `Vehicle`은 실제 Hanwha 세션에서 모두 나타난다.
- 저장 세션 분석 결과 parser-noise label도 존재하지만, 안정적인 normalized label에 비하면 드물다.
- `Cayr`, `Caar`, `Hukuman` 같은 값은 안정적인 제품 카테고리가 아니라 parser-noise로 보는 것이 맞다.

### `score`
의미:
- camera-reported likelihood / confidence 값이다.

해석:
- 같은 object id가 시간에 따라 어떻게 반복 감지되는지 비교할 때 유용하다.
- 앱이 계산한 confidence가 아니라 카메라가 보낸 점수로 읽어야 한다.

## 실무 해석 가이드
### overlay가 있을 때
- 카메라가 object metadata를 보냈다.
- 앱이 렌더링할 만큼 충분한 detail을 파싱했다.

### overlay는 없지만 `raw=seen`일 때
- `parsed`, `fresh`, recent metadata summary를 함께 본다.
- event-only metadata, 현재 복구 불가능한 fragmented payload, stale overlay state가 주요 원인 후보다.

### `malformed-payload`가 보일 때
- metadata 부재로 곧바로 해석하면 안 된다.
- 먼저 `objects`를 확인해야 한다.
- `objects > 0`이면 그 payload는 실무적으로 여전히 유효했다는 뜻이다.

### `recovered-continuation`이 보일 때
- continuation handling이 실제로 작동 중이라는 근거로 본다.
- status가 `success`면 continuation chain이 완성되었다는 뜻이다.
- status가 `malformed-payload`면 여전히 중간 fragment 상태로 끝났다는 뜻이다.

### 같은 객체가 화면에 계속 남아 있을 때
- overlay label의 `ObjectId`를 비교한다.
- 같은 `ObjectId`가 유지되면 카메라가 하나의 track을 유지하고 있을 가능성이 높다.
- 같은 물체인데 `ObjectId`가 자주 바뀌면 track instability 가능성이 있다.

## 현재 UI 가이드
권장 operator-facing 표현:
- `status`와 단순화된 parser-health category를 함께 보여준다.
- raw parser message는 log와 forensic view에 남긴다.
- parser-noise label은 안정적 class metric과 분리해서 다룬다.
- track continuity 확인을 위해 overlay label에 `ObjectId`를 포함한다.
- 예시는 다음처럼 compact하게 유지한다:
  - `success | recovered-continuation | objects=9 | Car #169173 (85%), Human #169375 (82%)`

## 다음 레퍼런스 작업
다음 단계에서 유용한 확장:
- raw XML field와 parsed runtime field의 대응 관계 정리
- Qt shell에서 parser-health category count를 직접 노출
- raw camera label이 어떤 과정을 거쳐 normalized runtime label이 되는지 문서화

