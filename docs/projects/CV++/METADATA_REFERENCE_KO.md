# CV++ 메타데이터 레퍼런스

## 문서 정보
- 버전: `v0.1`
- 상태: `초안`
- 작성일: `2026-03-24`
- 최종 수정일: `2026-03-24`
- 작성 주체: `Software Engineer Agent`

## 목적
이 문서는 CV++가 현재 표시하는 parsed metadata 값이 무엇을 의미하는지 정리해서, SungHwan이 런타임 화면, recent metadata, 세션 로그를 일관된 기준으로 해석할 수 있게 하는 것을 목표로 한다.

## Parsing Status 값
### `success`
의미:
- payload 안에 object metadata가 있었고, parser가 기대한 object block을 정상적으로 복구했다는 뜻이다.

해석:
- 카메라가 사용 가능한 object metadata를 보냈다는 강한 근거다.
- overlay와 session metrics에 그대로 쓰기 좋은 상태다.

### `no-objects`
의미:
- payload 자체는 파싱 가능했지만 `<tt:Object>` block이 없었다는 뜻이다.

해석:
- 카메라가 metadata는 보냈지만, 해당 payload에는 object-bearing metadata가 없었다고 볼 수 있다.
- event-only 또는 non-object metadata와 대응하는 경우가 많다.

### `malformed-payload`
의미:
- metadata는 도착했지만, XML payload가 fragment되었거나, incomplete하거나, clean하게 닫히지 않았다는 뜻이다.

해석:
- 이 값은 metadata가 없었다는 뜻이 아니다.
- 보통 parser가 partial payload에서 object를 복구하려고 했다는 뜻에 가깝다.
- object field가 복구되었다면 overlay와 evidence 용도로는 여전히 유용하다.

현재 관측:
- live 세션에서 가장 자주 보이는 status다.
- 따라서 희귀 예외라기보다 현재 runtime의 일반 조건으로 보는 것이 맞다.

## 지금까지 관측된 Message 값
### `""` (빈 메시지)
의미:
- parser가 추가 설명을 붙이지 않은 상태다.

해석:
- 운영 UI 기준 정보량은 낮다.
- operator-facing summary에서는 생략해도 되는 경우가 많다.

### `Object block did not close cleanly`
의미:
- object block이 XML상 정상적으로 닫히기 전에 잘렸다는 뜻이다.

해석:
- parser health를 보여주는 중요한 신호다.
- object field 일부는 여전히 복구될 수 있다.
- 로그와 parser-health metrics에서는 유지할 가치가 있다.

### `XML start marker not found`
의미:
- 현재 payload chunk 안에 XML 시작 마커가 없었다는 뜻이다.

해석:
- transport fragmentation이나 chunk boundary 문제를 시사한다.
- payload assembly 동작을 이해하는 데 유용하다.

### `No <tt:Object> blocks found`
의미:
- payload 안에 metadata는 있었지만 object block은 없었다는 뜻이다.

해석:
- overlay가 없지만 metadata traffic은 있었다는 근거로 유용하다.

### `Objects parsed successfully`
의미:
- parser가 object block을 clean하게 복구했다는 뜻이다.

해석:
- 강한 positive signal이다.
- success-rate metrics에는 유용하지만, operator UI에서는 너무 반복적일 수 있다.

## Object 필드
### `id`
의미:
- 카메라가 부여한 object identifier (`ObjectId`)다.

해석:
- 반복 detection이 같은 tracked object인지 확인할 때 사용한다.
- 같은 `id`가 여러 frame에서 반복되면, 새로운 고유 객체가 아니라 하나의 camera-reported object에 대한 반복 detection으로 보는 것이 맞다.

### `type`
의미:
- parser 정규화 이후의 camera-reported object class다.

현재 자주 보이는 값:
- `Car`
- `Human`
- `Vehicle`
- `Bicycle`
- `Head`
- `Bus`
- `Truck`
- `Motorcycle`
- `Unknown`

비고:
- 실제 세션에서 `Car`와 `Vehicle`이 둘 다 나타난다.
- fragmented payload 때문에 `Hu`, `Ca`, `Caar` 같은 깨진 label이 남을 수 있는데, 이는 안정적인 class라기보다 parser-noise로 보는 것이 맞다.

### `score`
의미:
- 카메라가 보낸 likelihood / confidence 값이다.

해석:
- 같은 object id가 시간에 따라 어떻게 반복 검출되는지 볼 때 유용하다.
- 앱이 계산한 confidence가 아니라 카메라가 생성한 score로 읽어야 한다.

## 실전 해석 가이드
### overlay가 있을 때
- 카메라가 object metadata를 보냈고
- 앱이 그것을 충분히 파싱해서
- 화면까지 반영한 상태다.

### overlay는 없지만 `raw=seen`일 때
- `parsed` count와 recent metadata를 같이 본다.
- 가능한 원인:
  - event-only metadata
  - recoverable object가 없는 malformed payload
  - stale overlay state

### `malformed-payload`가 보일 때
- 즉시 실패로 해석하면 안 된다.
- 먼저 `objects > 0`인지 확인해야 한다.
- `objects > 0`이면 운영상 유효한 payload였다고 볼 수 있다.

### 같은 객체가 화면에 계속 있을 때
- overlay 라벨의 `ObjectId`를 비교한다.
- 같은 `ObjectId`가 계속 유지되면 카메라가 하나의 track으로 보고 있다는 뜻이다.
- 같은 객체처럼 보이는데 `ObjectId`가 자주 바뀌면 track instability 가능성이 있다.

## 현재 UI 표현 권고
운영 UI에는 다음 방향이 적절하다.
- `status`는 단순화해서 보여주기
- 빈 `message`는 숨기기
- `Object block did not close cleanly` 같은 중요한 parser note는 유지하기
- overlay 라벨에는 `ObjectId`를 함께 보여주기
- recent metadata는 아래처럼 compact summary를 우선 사용하기
  - `malformed-payload | objects=2 | Car #165420 (57%), Vehicle #165454 (91%)`

## 다음 레퍼런스 작업
이 문서의 다음 확장 방향은 아래가 좋다.
- raw XML field와 parsed runtime field 대응 관계 정리
- 어떤 class label이 카메라 원문인지, 어떤 값이 parser normalization 결과인지 정리
- `Car`, `Human`, `Bicycle`, `Vehicle`, event-only payload 실제 예시 추가
