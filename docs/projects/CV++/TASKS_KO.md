# CV++ 작업 계획

## 문서 정보
- 버전: `v1.1`
- 상태: `마일스톤 3 진행 중, thermal metadata 미래 트랙 추가`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-25`
- 작성 주체: `Software Engineer Agent`

## 변경 이력
| 날짜 | 버전 | 변경 내용 |
| --- | --- | --- |
| 2026-03-18 | v0.1 | CV++ v0.1 MVP 구현 계획 초안 작성 |
| 2026-03-18 | v0.2 | 마일스톤 1 완료 및 초기 로그 경로 결정 반영 |
| 2026-03-18 | v0.3 | parser transparency, parsed summary, fixture candidate capture를 포함한 마일스톤 2 착수 |
| 2026-03-19 | v0.4 | `profile2`, `profile4` 고해상도 스트림 1차 검증 결과 반영 |
| 2026-03-19 | v0.5 | 고해상도 조사 재개 및 mixed pipeline 불안정 반영 |
| 2026-03-20 | v0.6 | 잘못된 profile별 metadata 결론을 정정하고 full-app orchestration 이슈로 초점 이동 |
| 2026-03-23 | v0.7 | 기반 마일스톤을 닫고, 다음 마일스톤을 metadata evidence와 performance visibility 중심으로 재정의 |
| 2026-03-23 | v0.8 | 현재 마일스톤을 유지한 채 Qt + SQLite 전환 트랙을 추가 |
| 2026-03-24 | v0.9 | 첫 live Qt verification shell slice를 완료하고, 다음 단계를 Qt polish와 SQLite 준비로 좁힘 |
| 2026-03-25 | v1.0 | Hanwha metadata baseline 분석 문서와 SQLite 저장 요구사항 문서를 추가하고, 다음 단계를 parser-health visibility와 SQLite foundation 작업으로 좁힘 |
| 2026-03-25 | v1.1 | thermal camera metadata 검증을 위한 deferred future milestone을 추가 |

## 요약
프로젝트는 초기 RTSP bring-up 단계를 넘어섰다. 현재 기준선은 video 수신, metadata 수집, 다중 객체 파싱, 다중 객체 overlay까지 가능하다. 다음 마일스톤은 사용자가 metadata evidence, parser health, metadata performance를 직접 판단할 수 있게 만드는 데 집중해야 한다.

## 마일스톤 1: 최소 런타임 골격
상태: 완료.

완료된 작업:
- RTSP URL, 헤더, latency, output 경로용 TOML 설정 추가
- `main.cpp`에서 config loading 분리
- raw metadata를 plain text로 기록하는 session logger 추가
- 정규화된 in-memory metadata 구조 정의
- UI 재설계 없이도 빌드가 유지되는지 확인

## 마일스톤 2: Metadata Capture 및 Parse Transparency
상태: 완료.

완료된 작업:
- raw metadata를 파싱 전에 로그에 기록
- parse status를 `success`, `unknown-pattern`, `no-objects`, `malformed-payload`로 분류
- `parsed_summary.log`에 parsed summary 기록
- `config.toml`로 실제 세션 fixture candidate 저장 가능
- 정상 종료 시 RTSP method를 기록하고 `PAUSE`, `TEARDOWN` 확인 가능
- 카메라 동작 검증용 `metadata_probe` 추가
- app metadata session이 선택한 auxiliary video track을 실제 소비하도록 수정
- fragment된 metadata 처리 개선으로 live 세션에서 multi-object parsing 복구
- live 실행에서 동시에 여러 객체 overlay가 다시 표시됨

현재 확인된 결과:
- `profile2`, `profile4` 모두 object-bearing metadata를 생성할 수 있음
- 앱은 동시에 여러 객체를 파싱하고 표시할 수 있음
- 최근 세션에서 `Car`, `Human`, `Bicycle`가 의미 있게 감지됨
- 기존의 profile별 metadata 동작 차이 결론은 정정됨

## 마일스톤 3: Metadata Evidence 및 Performance Visibility
상태: 진행 중.

목표:
아래 두 질문에 런타임에서 직접 답할 수 있게 만든다.
1. 지금 보고 있는 객체에 대해 카메라가 실제 metadata를 보냈는가?
2. 세션 전체 기준으로 카메라 metadata 성능은 어느 정도인가?

작업:
- raw metadata seen, parsed object count, overlay object count, metadata age를 보여주는 evidence banner/panel 추가
- 객체 타입별 detection 수와 unique object ID 수 session metric 추가
- camera-reported `ObjectId`를 사용해 반복 detection event와 unique tracked object를 구분
- malformed payload rate와 parser health를 사람이 확인 가능한 형태로 노출
- overlay가 없는 이유가 metadata 부재인지, parse loss인지, display-state loss인지 구분 가능하게 만들기

첫 번째 vertical slice에서 완료된 작업:
- raw-seen, parsed-count, overlay-count, metadata age를 보여주는 runtime evidence banner 추가
- 객체 타입별 detection 수와 unique object ID 수를 session-end metrics로 기록
- shared runtime state에서 raw payload count, parsed payload count, malformed payload count, event-only payload count 추적 시작
- OSD-first view를 최소 verification layout으로 교체: 왼쪽 video, 오른쪽 evidence/metadata panel
- IP, username, password, profile 입력용 runtime connection setup UI 추가
- 연결, video 표시, overlay label 렌더링, evidence / metrics / recent metadata panel 업데이트가 가능한 첫 live Qt shell slice 완료

현재 analysis/documentation slice에서 완료된 작업:
- 저장 세션을 기준으로 현재 Hanwha metadata baseline을 문서화했고, parser-health ratio, stable class baseline, parser-noise observation을 정리함
- session log를 local review data로 바꾸는 데 필요한 최소 SQLite 저장 요구사항을 문서화함
- metadata analysis 문서를 현재 parser taxonomy인 `truncated-object-fragment`, `recovered-continuation`, `metadata-without-objects` 기준으로 정렬함

Milestone 3 안에서의 다음 단계:
- 현재 taxonomy를 사용해 Qt shell에 parser-health count 노출
- parser-noise label은 forensic view에는 보이되, headline metric에서는 별도 그룹으로 묶기
- 이후 SQLite ingestion이 기계적으로 가능하도록 session summary 형식을 새 baseline 문서와 맞추기

완료 기준:
- 앱이 manual log inspection 없이 evidence 상태를 노출한다
- 사용자가 한 세션 안에서 반복 detection과 unique object count를 비교할 수 있다
- 어떤 객체가 안 보일 때, 카메라가 metadata를 안 보낸 것인지 단순히 화면에 안 그려진 것인지 구분할 수 있다

## 마일스톤 4: Overlay State Isolation
목표: freshness와 stale-object 동작을 더 신뢰 가능하고 유지보수 가능하게 만든다.

작업:
- overlay state handling을 `main.cpp`에서 분리
- freshness timeout과 stale-clear rule을 중앙화
- capture된 sample 기준으로 disappearing object 제거가 맞는지 검증

## 마일스톤 5: Minimal Verification View
목표: 제품 UI로 범위를 확장하지 않으면서도, operator가 한 화면에서 검증할 수 있게 한다.

작업:
- live video + overlay 표시
- recent parsed metadata summary 표시
- recent raw metadata line 또는 raw metadata panel 표시
- connection 및 reconnect 상태 표시

## 마일스톤 6: Basic Session Robustness
목표: 일반적인 카메라 불안정 상황에서도 v0.1을 usable하게 만든다.

작업:
- 단순 automatic reconnect 추가
- reconnect attempt와 session state change를 로그에 기록
- reconnect 상태가 verification view에 보이는지 확인

## 마일스톤 7: Qt Verification UI Transition
목표: operator-facing UI를 임시 OpenCV-only view에서 유지보수 가능한 desktop UI 플랫폼으로 옮긴다.

작업:
- 기존 runtime core를 host하는 작은 Qt shell application 정의
- RTSP session, parser, logging module을 현재 C++ 구현에서 재사용 가능하게 유지
- 임시 connection setup canvas를 proper Qt connection form으로 교체
- 현재 오른쪽 panel 렌더링을 Qt widget 기반 evidence, metrics, recent metadata panel로 교체
- 현재 verification workflow를 유지하면서 readability, DPI handling, layout quality를 개선

첫 scaffold slice에서 완료된 작업:
- 로컬에 Qt 6.8.3 MSVC 2022 64-bit 설치
- build에 별도 `CVPP_QtShell` target 추가
- connection form 영역과 verification layout placeholder를 가진 최소 Qt shell 생성
- 현재 runtime core는 건드리지 않고 다음 UI layer 준비

두 번째 scaffold slice에서 완료된 작업:
- Qt connection form을 runtime-backed session startup에 연결
- Qt shell을 `VideoRtspSession`, `MetadataRtspSession`, `SharedAppState`에 연결
- video placeholder를 live runtime frame surface와 overlay preview로 교체
- evidence, metrics, recent metadata panel이 shared runtime state에서 업데이트되도록 연결

Qt 전환 안에서의 다음 단계:
- connection UX와 state messaging 개선
- panel density와 visual hierarchy 조정
- Qt shell을 main operator surface로 검증하는 동안 OpenCV view는 fallback으로만 유지

## 마일스톤 8: SQLite Session Review Foundation
목표: session metric과 이후 metadata review를 위한 실용적인 local persistence layer를 추가한다.

작업:
- session summary, parsed detection, unique object metric을 위한 최소 SQLite schema 정의
- 기존 plain text log와 함께 session-end metrics를 저장
- SQLite가 추가되어도 plain log를 raw evidence로 유지
- 이후 review/comparison screen에 필요한 data model 준비

planning 단계에서 완료된 작업:
- `docs/projects/CV++/SQLITE_STORAGE_REQUIREMENTS.md` 초안 작성
- 첫 schema를 `sessions`, `session_artifacts`, `parsed_payloads`, `parsed_objects`, `session_type_metrics`로 좁힘

권장 순서:
1. Qt verification shell polish 마무리
2. UI와 session summary에서 metadata performance metric 심화
3. SQLite foundation 시작

## 마일스톤 9: Thermal Camera Metadata 검증
상태: deferred future track.

목표: 현재 visible-light Hanwha observability 경로가 충분히 안정화된 뒤, CV++가 thermal camera metadata도 검증할 수 있도록 verification workflow를 확장한다.

작업:
- 대상 thermal camera의 RTSP 및 metadata 전달 특성 확인
- 대표적인 thermal metadata fixture와 session log 수집
- 현재 Hanwha 기준선과 비교해 payload format 및 transport 차이를 문서화
- 현재 parser normalization을 재사용할 수 있는지, 아니면 별도 thermal metadata 경로가 필요한지 검증
- visible-light 세션과 thermal metadata 세션을 비교하기 위해 필요한 UI, review, storage 조정 사항 정의

시작 조건:
- 현재 Milestone 3와 Milestone 8 기반이 충분히 안정화되어, thermal 작업이 현재 observability 기준선을 흐리지 않을 때 시작
## 핵심 우려
- observability가 제대로 작동하기 전에 큰 refactor를 하지 말 것
- raw metadata evidence가 충분히 신뢰되기 전에 polished UI를 먼저 만들지 말 것
- 각 마일스톤은 독립적으로 runnable해야 함
- full app에서 결론 내리기 전에 `metadata_probe`를 control experiment로 사용할 것
- SungHwan이 practical C++ 이해를 쌓을 수 있도록 implementation은 학습 가능해야 함

## 반영된 결정
- 기본 로그 경로: `output/session-YYYYMMDD-HHMMSS/` 기반 세션 폴더
- v0.1 raw metadata log 형식: 세션당 single plain file
- 카메라 동작 검증의 control experiment: `metadata_probe`
- 다음 마일스톤은 visual polish보다 metadata evidence와 metadata performance를 우선
- 중기 UI 방향: Qt desktop application
- 중기 storage 방향: local session review용 SQLite

## 참고 문서
- `docs/projects/CV++/HIGH_RESOLUTION_PROFILE_INVESTIGATION.md`
- `docs/projects/CV++/HANWHA_METADATA_BASELINE_ANALYSIS.md`
- `docs/projects/CV++/MILESTONE_REVIEW.md`
- `docs/projects/CV++/SQLITE_STORAGE_REQUIREMENTS.md`
- `docs/projects/CV++/MESSAGE_INVESTIGATION.md`
- `docs/projects/CV++/METADATA_REFERENCE.md`
- `docs/projects/CV++/CPP_LEARNING_GUIDE.md`

