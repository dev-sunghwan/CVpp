# CV++ 작업 계획

## 문서 정보
- 버전: `v0.2`
- 상태: `마일스톤 1 완료`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-18`
- 작성 주체: `Software Engineer Agent`

## 변경 이력
| 날짜 | 버전 | 변경 내용 |
| --- | --- | --- |
| 2026-03-18 | v0.1 | CV++ v0.1 MVP를 위한 초기 구현 계획 작성 |
| 2026-03-18 | v0.2 | 마일스톤 1 완료 및 초기 로그 경로 결정 반영 |

## 요약
이 계획은 승인된 PM 범위와 Tech Lead 아키텍처를 따른다. 순서는 의도적이다. 먼저 설정과 raw metadata 관측 가능성을 확보하고, 그 다음 parser 신뢰성을 높이고, 마지막으로 운영자가 빠르게 검증할 수 있는 최소 UI를 더한다.

## 마일스톤 1: 최소 런타임 뼈대
상태: 완료.

완료된 작업:
- RTSP URL, 헤더, latency, 로그 루트를 담는 TOML 설정 파일 추가
- `main.cpp`에서 config loading 분리
- raw metadata를 plain text 파일에 기록하는 session logger 추가
- 정규화된 in-memory metadata object 구조 정의
- UI 재설계 없이 빌드 유지 확인

완료 결과:
- 스트림 설정이 더 이상 하드코딩되어 있지 않다
- 앱이 raw metadata 로그 파일을 생성할 수 있다
- config loading 또는 log 생성 실패가 콘솔이나 로그에 보인다
- 각 실행은 세션별 output 폴더를 생성한다

## 마일스톤 2: Metadata Capture 및 Parse Transparency
목표: raw metadata와 parse 결과를 동시에 관측 가능하게 만든다.

작업:
- raw metadata payload가 파싱 전에 먼저 로그를 거치도록 연결
- parse success, parse failure, unknown pattern 케이스를 명시적으로 드러내기
- 실제 카메라 기반의 소규모 sample fixture 수집
- 대표 샘플에 대한 기본 확인 추가: 정상 객체, 빈 장면, 객체 소멸, class pattern 변형

완료 기준:
- 같은 세션에 대해 raw와 parsed 출력을 비교할 수 있다
- parser failure가 더 이상 조용히 묻히지 않는다
- 실제 Hanwha metadata 기반 sample fixture가 존재한다

## 마일스톤 3: Overlay State 분리
목표: freshness와 stale-object 동작을 더 신뢰 가능하고 유지보수 가능하게 만든다.

작업:
- overlay state handling을 `main.cpp`에서 분리
- freshness timeout과 stale-clear 규칙을 한 곳으로 모으기
- 수집된 샘플을 기준으로 객체 소멸 시 정상 제거되는지 확인

완료 기준:
- overlay update가 메인 진입 흐름 밖에서 처리된다
- stale detection이 일관되게 정리된다

## 마일스톤 4: 최소 검증 화면
목표: 제품 UI로 범위를 확장하지 않으면서 한 화면 검증 기능을 제공한다.

작업:
- overlay가 포함된 실시간 영상 표시
- 최근 parsed metadata summary 표시
- 최근 raw metadata 라인 또는 raw metadata 패널 표시
- 연결 상태 및 재연결 상태 표시

완료 기준:
- 사용자가 한 화면에서 영상, overlay, parsed output, raw evidence를 비교할 수 있다

## 마일스톤 5: 기본 세션 안정성
목표: 일반적인 카메라 불안정 상황에서도 v0.1이 사용 가능하도록 만든다.

작업:
- 단순 자동 재연결 동작 추가
- 재연결 시도와 세션 상태 변화를 로그에 기록
- 재연결 상태가 검증 화면에 보이도록 연결

완료 기준:
- 일시적인 스트림 끊김이 사용자에게 보이고, 앱이 자동으로 복구를 시도한다

## 핵심 우려 사항
- 관측성이 확보되기 전에는 큰 리팩터링을 피해야 한다
- raw metadata 근거가 신뢰 가능해지기 전에는 polished UI를 만들지 않는다
- 각 마일스톤은 독립적으로 실행 가능해야 한다

## 반영된 결정
- 기본 로그 경로 전략: `output/session-YYYYMMDD-HHMMSS/` 형태의 세션별 runtime output 폴더
- v0.1 raw metadata 로그 형식: 세션당 단일 plain file

## 권고
이제 바로 마일스톤 2로 넘어가는 것이 적절하다. 마일스톤 1은 완료됐고, 다음 실질 리스크는 설정 작업이 아니라 parser transparency다.

## 열린 질문
- 마일스톤 2에서 parse failure를 우선 로그에만 남길지, 최소 화면 상태 표시에도 반영할지?
- 실제 Hanwha 카메라 세션에서 어떤 구성을 첫 fixture 세트의 최소 기준으로 볼지?

## SungHwan 승인 요청
마일스톤 1에서 마일스톤 2로의 전환을 승인해 달라: metadata capture 및 parse transparency.
