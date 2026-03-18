# CV++ 작업 계획

## 문서 정보
- 버전: `v0.1`
- 상태: `초안`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-18`
- 작성 주체: `Software Engineer Agent`

## 변경 이력
| 날짜 | 버전 | 변경 내용 |
| --- | --- | --- |
| 2026-03-18 | v0.1 | CV++ v0.1 MVP를 위한 초기 구현 계획 작성 |

## 요약
이 계획은 승인된 PM 범위와 Tech Lead 아키텍처를 따른다. 순서는 의도적이다. 먼저 설정과 raw metadata 관측 가능성을 확보하고, 그 다음 parser 신뢰성을 높이고, 마지막으로 운영자가 빠르게 검증할 수 있는 최소 UI를 더한다.

## 마일스톤 1: 최소 런타임 뼈대
목표: 가장 작은 코드 이동으로 앱을 설정 가능하게 만들고 raw metadata 로그 준비를 마친다.

작업:
- RTSP URL, 헤더, latency, 로그 경로를 담는 TOML 설정 파일 추가
- `main.cpp`에서 config loading 분리
- raw metadata 한 줄 로그를 plain text 파일에 기록하는 단순 session logger 추가
- 파싱이 아직 부분적이더라도 정규화된 in-memory metadata event 형태 정의
- UI 재설계 없이도 기존 앱이 계속 빌드되고 실행되는지 확인

완료 기준:
- 스트림 설정이 더 이상 하드코딩되어 있지 않다
- 앱이 raw metadata 로그 파일을 생성할 수 있다
- config loading 또는 log 생성 실패가 콘솔이나 로그에 명확히 보인다

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

## 권고
즉시 마일스톤 1부터 시작하는 것이 적절하다. 이 단계가 가장 작으면서도 리스크를 줄이고, 이후 MVP 작업을 모두 열어준다.

## 열린 질문
- 기본 로그 경로는 프로젝트 디렉터리 아래가 좋은지, 런타임 생성 output 폴더가 좋은지?
- v0.1에서는 raw metadata 로그를 회전시킬지, 단순 세션 파일로 둘지?

## SungHwan 승인 요청
이 구현 순서를 승인해 달라. 특히 첫 마일스톤인 config externalization과 plain-file raw metadata logging부터 시작하는 방향을 승인해 달라.
