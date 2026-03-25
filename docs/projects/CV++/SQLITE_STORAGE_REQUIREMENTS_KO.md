# CV++ SQLite 저장 요구사항

## 문서 정보
- 버전: `v0.1`
- 상태: `초안`
- 작성일: `2026-03-25`
- 최종 수정일: `2026-03-25`
- 작성 주체: `Software Engineer Agent`

## 목적
이 문서는 Milestone 8에서 필요한 최소 SQLite 요구사항을 정의해서, 현재의 plain-file evidence 흐름을 대체하지 않으면서도 CV++가 log-only session review에서 실용적인 local review로 넘어갈 수 있게 하는 것을 목표로 한다.

## 현재 런타임 산출물
각 세션은 현재 다음 파일을 기록한다:
- `session.log`
  - lifecycle event
  - RTSP method log
  - parser-health message
  - session-end metrics summary
- `metadata_raw.xml.log`
  - timestamp가 붙은 raw XML payload evidence
- `parsed_summary.log`
  - timestamp가 붙은 parsed status 및 object summary

현재 completed-session summary line에는 이미 다음 값이 포함된다:
- `raw_samples`
- `parsed_payloads`
- `malformed`
- `event_only`
- `detection_events`
- `detections_by_type`
- `unique_ids_by_type`

## SQLite가 답할 수 있어야 하는 제품 질문
- 어떤 세션의 metadata 품질이 가장 좋고 나빴는가?
- 특정 세션은 object metadata가 많았는가, event-only metadata가 많았는가, 아니면 fragmented payload가 대부분이었는가?
- 한 세션에서 타입별 detection은 몇 번 발생했는가?
- 한 세션에서 타입별 unique camera-reported object ID는 몇 개였는가?
- 어떤 parsed payload가 어떤 object를 만들었는가?
- 특정 review 세션의 raw evidence는 어느 session folder와 plain log에 있는가?

## Milestone 8 범위
SQLite는 다음이어야 한다:
- local only
- single-user
- file-backed
- 현재 log flow에 추가되는 계층이지, 대체하는 계층이 아님

SQLite는 아직 다음이 되면 안 된다:
- raw XML의 source of truth
- multi-camera fleet database
- cloud 또는 server-backed system
- 대규모 event warehouse

## 필수 비기능 제약
- SQLite가 추가되어도 plain log는 계속 디스크에 남겨야 한다
- RTSP password 같은 credential은 저장하지 않는다
- desktop SQLite viewer로 직접 열어봐도 이해 가능한 단순한 schema를 유지한다
- 하나의 session folder가 하나의 SQLite session row와 깔끔하게 대응되어야 한다
- 가능하면 runtime insert는 append-only에 가깝게 유지한다

## 필수 핵심 엔터티
### `sessions`
세션 폴더당 1 row.

필수 필드:
- `session_id`
  - `session-20260324-163207` 같은 폴더명 기반
- `started_at`
- `ended_at`
- `output_dir`
- `camera_host`
- `profile_name`
- `metadata_enabled`
- `raw_samples`
- `parsed_payloads`
- `malformed_payloads`
- `event_only_payloads`
- `detection_events`
- `notes`
  - 이후 operator review용 optional free-text field

중요한 규칙:
- credential이 포함된 full RTSP URL은 저장하지 않는다

### `session_artifacts`
기존 plain log와 session row를 연결한다.

필수 필드:
- `session_id`
- `artifact_type`
  - 초기 예상 값: `session_log`, `raw_metadata_log`, `parsed_summary_log`
- `artifact_path`

목적:
- review tool이 SQLite summary data에서 raw evidence로 바로 이동할 수 있게 한다

### `parsed_payloads`
parsed summary line당 1 row.

필수 필드:
- `payload_id`
- `session_id`
- `observed_at`
- `parse_status`
- `parse_message`
- `object_count`
- `has_video_analytics`
  - 가능할 때 derived 또는 backfill
- `is_continuation_related`
  - v0.1에서는 parse message 기반 derived 값

이 table이 필요한 이유:
- Milestone 3와 Hanwha baseline은 최종 detection total만이 아니라 parser-health distribution 자체에 의존하고 있다

### `parsed_objects`
parsed object occurrence당 1 row.

필수 필드:
- `parsed_object_id`
- `payload_id`
- `session_id`
- `object_id`
- `normalized_type`
- `likelihood`
- `left`
- `top`
- `right`
- `bottom`

목적:
- session review, unique-object metric, 이후 timeline/filter view를 지원한다

### `session_type_metrics`
세션과 normalized type 조합당 1 row.

필수 필드:
- `session_id`
- `normalized_type`
- `detection_count`
- `unique_object_count`

목적:
- 현재 session-end metrics line을 거의 그대로 반영한다
- 이 값들이 `parsed_objects`에서 계산 가능하더라도 review query 경로를 단순하게 유지한다

## 권장 최소 Schema 형태
첫 구현에서 권장하는 table:
1. `sessions`
2. `session_artifacts`
3. `parsed_payloads`
4. `parsed_objects`
5. `session_type_metrics`

이유:
- 현재 마일스톤 질문을 답하기에 충분하다
- premature한 세분화를 피할 수 있다
- runtime이 이미 쓰고 있는 log 파일 구조와 잘 맞는다

## SQLite에서 쉽게 계산되어야 하는 파생 지표
SQLite 기반 review는 다음 계산을 쉽게 할 수 있어야 한다:
- 세션별 malformed-payload rate
- 세션별 event-only rate
- 타입별 detection event 수
- 타입별 unique object ID 수
- 안정적인 normalized class 집합 밖의 parser-noise label이 나온 세션
- metadata는 존재했지만 clean payload는 드문 세션

## 데이터 소유권 규칙
- raw XML은 `metadata_raw.xml.log`에 남긴다
- SQLite는 review data와 structured summary를 저장한다
- structured data와 log가 충돌하면 debugging 기준 source of truth는 log다

## 적재 방식 기대치
첫 구현에서는 다음 정도면 충분하다:
- `SessionLogger` 초기화 시 session-start row 생성
- metadata 수신 시 parsed payload row와 parsed object row append
- 정상 종료 시 final session summary field update
- `session_type_metrics`는 이미 추적 중인 runtime aggregate를 이용해 session end에 기록

runtime write가 실패해도:
- 앱은 plain log 기록을 계속해야 한다
- SQLite persistence는 세션을 막지 않고 graceful degradation 되어야 한다

## 완료 기준
Milestone 8 storage foundation은 다음이 가능해지면 완료다:
- 모든 completed session을 `session_id`로 SQLite에서 찾을 수 있다
- text log를 읽지 않고도 SQLite에서 session summary count를 볼 수 있다
- SQLite가 한 세션의 detections by type, unique IDs by type를 답할 수 있다
- SQLite가 한 세션의 parsed payload status와 message count를 보여줄 수 있다
- SQLite row가 같은 session folder의 plain log 파일로 다시 연결된다
- database-backed review data가 추가되어도 raw XML log는 그대로 유지된다

## 이후로 미루는 항목
- full raw XML blob를 SQLite 안에 저장하는 것
- 카메라 간 multi-session object tracking
- 하나의 `ObjectId` duration 추정
- 단순 session note를 넘어서는 review annotation
- database sync 또는 remote storage

## 권고
SQLite는 기존 session folder 위에 얹는 local review index로 구현해야 한다. 첫 schema는 의도적으로 작게 유지하고, runtime이 오늘 이미 알고 있는 데이터 구조를 그대로 반영하는 것이 맞다.
