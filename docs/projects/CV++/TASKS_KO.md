# CV++ 작업 계획

## 문서 정보
- 버전: `v0.9`
- 상태: `마일스톤 3 첫 vertical slice 완료, Qt 전환 진행 중`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-24`
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

## 요약
프로젝트는 초기 RTSP bring-up 단계를 넘어섰다. 현재 기준선은 video 수신, metadata 수집, 다중 객체 파싱, 다중 객체 overlay까지 가능하다. 다음 마일스톤은 사용자가 metadata evidence와 metadata performance를 직접 판단할 수 있게 만드는 데 집중해야 한다.

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
- raw metadata seen, parsed object count, overlay object count, metadata age를 보여주는 evidence banner 또는 panel 추가
- 객체 타입별 detection 수와 unique object ID 수를 보여주는 session metrics 추가
- 반복 detection event와 고유 tracked object를 카메라의 `ObjectId` 기준으로 구분
- malformed payload 비율과 parser health를 사람이 해석 가능한 형태로 노출
- overlay가 없을 때 그 원인이 missing metadata인지, parse loss인지, display-state loss인지 구분 가능하게 만들기

첫 번째 vertical slice에서 완료된 작업:
- raw-seen, parsed-count, overlay-count, metadata age를 보여주는 런타임 evidence banner 추가
- 세션 종료 시 detection 수와 unique object ID 수를 로그로 남기는 session metrics 추가
- raw payload count, parsed payload count, malformed payload count, event-only payload count를 shared state에서 추적 시작
- OSD 중심 표시를 최소 검증 화면으로 바꿔 좌측 비디오, 우측 evidence/metadata 패널 구조를 추가
- IP, username, password, profile 입력을 위한 런타임 연결 설정 UI 추가
- live frame, overlay label, evidence, metrics, recent metadata를 실제로 표시하는 첫 Qt shell slice 완료

완료 기준:
- 사용자가 로그를 직접 열지 않아도 evidence 상태를 알 수 있음
- 한 세션에서 반복 detection 수와 고유 객체 수를 함께 비교 가능함
- overlay 부재가 카메라 미전송인지 앱 손실인지 구분 가능함

## 마일스톤 4: Overlay State 분리
목표: freshness와 stale-object 동작을 더 신뢰 가능하고 유지보수 가능하게 만든다.

작업:
- overlay state handling을 `main.cpp`에서 분리
- freshness timeout과 stale-clear 규칙 중앙화
- 수집한 샘플 기준으로 객체 소멸 처리 검증

## 마일스톤 5: 최소 검증 화면
목표: 제품 UI로 확장하지 않고, 한 화면 검증 기능을 제공한다.

작업:
- overlay가 포함된 실시간 영상 표시
- 최근 parsed metadata summary 표시
- 최근 raw metadata 라인 또는 raw metadata 패널 표시
- 연결 상태 및 재연결 상태 표시

## 마일스톤 6: 기본 세션 안정성
목표: 일반적인 카메라 불안정 상황에서도 v0.1이 사용 가능하도록 만든다.

작업:
- 단순 자동 재연결 추가
- 재연결 시도 및 세션 상태 변화 로그 기록
- 재연결 상태가 검증 화면에 보이도록 연결

## 마일스톤 7: Qt 검증 UI 전환
목표: 임시 OpenCV-only 화면에서 벗어나 유지보수 가능한 데스크톱 UI 플랫폼으로 옮긴다.

작업:
- 기존 runtime core를 호스팅하는 작은 Qt shell 애플리케이션 정의
- 현재 C++ RTSP session, parser, logging 모듈을 재사용 가능한 형태로 유지
- 임시 connection setup canvas를 정식 Qt connection form으로 교체
- 현재 우측 패널 렌더링을 evidence, metrics, recent metadata용 Qt widget으로 교체
- 검증 workflow는 유지하면서 가독성, DPI 대응, 레이아웃 품질을 개선

첫 번째 scaffold slice에서 완료된 작업:
- 로컬에 Qt 6.8.3 MSVC 2022 64-bit 설치
- 빌드에 별도 `CVPP_QtShell` 타깃 추가
- connection form 영역과 verification layout placeholder를 가진 최소 Qt shell 생성
- 다음 UI 계층을 준비하면서도 현재 runtime core는 건드리지 않음

두 번째 scaffold slice에서 완료된 작업:
- Qt connection form을 실제 runtime session 시작과 연결
- Qt shell이 `VideoRtspSession`, `MetadataRtspSession`, `SharedAppState`를 직접 사용하도록 연결
- video placeholder를 실제 runtime frame surface와 overlay preview로 교체
- evidence, metrics, recent metadata 패널을 shared runtime state에서 갱신하도록 연결

Qt 전환의 다음 단계:
- connection UX와 상태 문구 정리
- 패널 밀도와 시각적 위계 개선
- OpenCV view는 fallback으로만 유지하면서 Qt shell을 주 운영 화면으로 검증

## 마일스톤 8: SQLite 세션 리뷰 기반
목표: 세션 metrics와 이후 metadata review를 위한 현실적인 로컬 persistence 계층을 추가한다.

작업:
- session summary, parsed detection, unique object metric용 최소 SQLite schema 정의
- 세션 종료 metrics를 기존 plain text 로그와 함께 SQLite에도 저장
- SQLite 추가 이후에도 plain log는 raw evidence로 유지
- 이후 review / comparison 화면에 필요한 데이터 모델을 먼저 준비

권장 순서:
1. Qt verification shell polish 마무리
2. UI 및 session summary의 metadata performance metrics 확장
3. SQLite foundation 착수

## 핵심 우려
- 관측성이 확보되기 전에 큰 리팩터링을 하지 않는다.
- raw metadata 근거가 신뢰되기 전에는 polished UI를 만들지 않는다.
- 각 마일스톤은 독립적으로 실행 가능해야 한다.
- full app 해석 전에 `metadata_probe`를 control experiment로 사용한다.
- SungHwan이 이 프로젝트를 통해 실용적인 C++ 이해를 쌓을 수 있도록 구현의 학습 가능성을 유지한다.

## 반영된 결정
- 기본 로그 경로: `output/session-YYYYMMDD-HHMMSS/` 형태의 세션별 output 폴더
- v0.1 raw metadata 로그 형식: 세션당 단일 plain file
- `metadata_probe`는 camera-behavior validation의 control experiment이다
- 다음 마일스톤은 visual polish보다 metadata evidence와 performance를 우선한다
- 중기 UI 방향: Qt 데스크톱 애플리케이션
- 중기 저장 방향: 로컬 session review를 위한 SQLite

## 참고 문서
- `docs/projects/CV++/HIGH_RESOLUTION_PROFILE_INVESTIGATION.md`
- `docs/projects/CV++/MILESTONE_REVIEW_KO.md`
- `docs/projects/CV++/CPP_LEARNING_GUIDE.md`
