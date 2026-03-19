# CV++ 작업 계획

## 문서 정보
- 버전: `v0.4`
- 상태: `마일스톤 2 진행 중`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-19`
- 작성 주체: `Software Engineer Agent`

## 변경 이력
| 날짜 | 버전 | 변경 내용 |
| --- | --- | --- |
| 2026-03-18 | v0.1 | CV++ v0.1 MVP 구현 계획 초안 작성 |
| 2026-03-18 | v0.2 | 마일스톤 1 완료 및 초기 로그 경로 결정 반영 |
| 2026-03-18 | v0.3 | parser transparency, parsed summary, fixture candidate capture를 포함한 마일스톤 2 착수 |
| 2026-03-19 | v0.4 | `profile2`, `profile4` 고해상도 스트림 검증 완료 반영 |

## 요약
이 계획은 승인된 PM 범위와 Tech Lead 아키텍처를 따른다. 순서는 의도적이다. 먼저 설정과 raw metadata 관측성을 확보하고, 그 다음 parser 신뢰성을 높이며, 마지막으로 최소 검증 UI를 정리한다.

## 마일스톤 1: 최소 런타임 골격
상태: 완료.

완료된 작업:
- RTSP URL, 헤더, latency, output 경로용 TOML 설정 추가
- `main.cpp`에서 config loading 분리
- raw metadata를 plain text로 기록하는 session logger 추가
- 정규화된 in-memory metadata 구조 정의
- UI 재설계 없이도 빌드가 유지되는지 확인

완료 결과:
- 스트림 설정이 더 이상 하드코딩되지 않음
- raw metadata 로그 파일 생성 가능
- config 및 logging 실패가 콘솔과 로그에 드러남
- 실행마다 session 기반 output 폴더 생성

## 마일스톤 2: Metadata Capture 및 Parse Transparency
상태: 진행 중.

현재까지 구현:
- raw metadata를 파싱 전에 로그에 기록
- parse status를 `success`, `unknown-pattern`, `no-objects`, `malformed-payload`로 분류
- `parsed_summary.log`에 parsed summary 기록
- `config.toml`로 실제 세션 fixture candidate 저장 가능
- 현재 parse status를 화면 배너로 표시
- `profile2`, `profile4` 모두에서 고해상도 재생 검증 완료
- 정상 종료 시 RTSP method를 기록하고 `PAUSE`, `TEARDOWN` 확인 가능

남은 작업:
- 실제 Hanwha 세션에서 대표 fixture 세트 수집
- 실제 metadata에서 unknown pattern 노출 확인
- object overlay 로직에서 non-object event metadata 분리
- 빠르게 움직이는 차량에 대한 overlay 품질 개선

완료 기준:
- 같은 세션 기준으로 raw와 parsed 결과 비교 가능
- parser failure가 더 이상 조용히 묻히지 않음
- 실제 Hanwha metadata fixture 확보
- live 검증에 쓸 수 있을 만큼 object overlay 신뢰성 확보

## 마일스톤 3: Overlay State 분리
목표: freshness와 stale-object 동작을 더 신뢰 가능하고 유지보수 가능하게 만든다.

작업:
- overlay state handling을 `main.cpp`에서 분리
- freshness timeout과 stale-clear 규칙 중앙화
- 수집한 샘플 기준으로 객체 소멸 처리 검증

완료 기준:
- overlay update가 main entry flow 밖에서 처리됨
- stale detection이 일관되게 정리됨

## 마일스톤 4: 최소 검증 화면
목표: 제품 UI로 확장하지 않고, 한 화면 검증 기능을 제공한다.

작업:
- overlay가 포함된 실시간 영상 표시
- 최근 parsed metadata summary 표시
- 최근 raw metadata 라인 또는 raw metadata 패널 표시
- 연결 상태 및 재연결 상태 표시

완료 기준:
- 사용자가 한 화면에서 영상, overlay, parsed output, raw evidence를 비교 가능

## 마일스톤 5: 기본 세션 안정성
목표: 일반적인 카메라 불안정 상황에서도 v0.1이 사용 가능하도록 만든다.

작업:
- 단순 자동 재연결 추가
- 재연결 시도 및 세션 상태 변화 로그 기록
- 재연결 상태가 검증 화면에 보이도록 연결

완료 기준:
- 일시적 스트림 끊김이 보이고 앱이 자동 복구를 시도함

## 핵심 우려
- 관측성이 확보되기 전에 큰 리팩터링을 하지 않는다.
- raw metadata 근거가 신뢰되기 전에는 polished UI를 만들지 않는다.
- 각 마일스톤은 독립적으로 실행 가능해야 한다.

## 반영된 결정
- 기본 로그 경로: `output/session-YYYYMMDD-HHMMSS/` 형태의 세션별 output 폴더
- v0.1 raw metadata 로그 형식: 세션당 단일 plain file
- 고해상도 검증 기준선: `profile2`, `profile4` 모두 앱 내에서 유효

## 권고
마일스톤 2는 live parser transparency, 실제 metadata 샘플, object overlay 신뢰성 개선에 집중한다. 아직 reconnect나 화면 레이아웃 확장으로 범위를 넓히지 않는다.

## 병행 조사
초기 고해상도 스트림 조사는 종료되었다.

참고 문서:
- `docs/projects/CV++/HIGH_RESOLUTION_PROFILE_INVESTIGATION.md`

## 열린 질문
- 실제 fixture capture는 개발 중 기본값으로 둘지, 계속 opt-in으로 둘지?
- 실제 Hanwha 세션에서 최소 fixture 세트를 무엇으로 볼지?
- 빠르게 움직이는 차량에 대해 freshness 또는 hold 규칙을 어떻게 둘지?

## SungHwan 승인 요청
실제 Hanwha 스트림 기준으로 fixture를 수집하고, non-object overlay 노이즈를 제거하며, 빠른 객체 overlay 동작을 개선하는 방향으로 마일스톤 2를 계속 진행한다.
