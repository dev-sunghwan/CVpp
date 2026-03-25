# CV++ 마일스톤 정리

## 문서 정보
- 버전: `v0.3`
- 상태: `업데이트됨`
- 작성일: `2026-03-23`
- 최종 수정일: `2026-03-25`
- 작성 주체: `PM / Tech Lead / Software Engineer Agents`

## 요약
현재 마일스톤은 여전히 성공적인 기반 구축 마일스톤으로 볼 수 있지만, 이제 다음 단계로 넘어가기 위한 metadata analysis 기준선이 더 분명해졌다. 최근 작업으로 실제 제품 목표를 다시 분명히 했고, profile별 metadata 동작에 대한 잘못된 결론을 바로잡았으며, RTSP 제어 경로를 다음 개발이 가능할 정도로 안정화했고, 메인 앱에서 다중 객체 overlay를 복구했으며, 현재 Hanwha parser-health 기준선과 이후 local review를 위한 SQLite 저장 요구사항까지 문서화했다.

## 이번 마일스톤에서 달성한 것
- `config.toml` 기반 RTSP 설정 외부화
- `output/session-.../` 기반 세션 로그 구조 도입
- 세션별 raw metadata 및 parsed summary 저장
- 정상 종료 시 `PAUSE`, `TEARDOWN` 확인
- 카메라 동작 검증용 `metadata_probe` 도입
- `profile2`, `profile4` metadata 의미가 다르다는 기존 잘못된 결론 정정
- 메인 앱 metadata session이 선택한 auxiliary video pad를 실제 소비하도록 수정
- live 실행에서 다중 객체 파싱 및 다중 객체 overlay 복구
- 저장 세션 기반 Hanwha metadata 기준선 문서화
- local session review를 위한 최소 SQLite 저장 요구사항 문서화

## 핵심 발견
- 문제의 핵심은 카메라 profile 자체가 아니었다
- 남아 있던 `gst-launch` 세션과 불완전한 probe가 잘못된 해석을 만들었다
- live metadata는 자주 fragment되므로 parser와 session 로직이 partial payload를 견뎌야 한다
- 메인 앱은 이제 여러 객체를 동시에 수신하고 화면에 표시할 수 있다
- 현재 로그의 객체 수는 고유 객체 수가 아니라 반복 detection event 수에 가깝다
- 새 parser taxonomy는 새로운 카메라 조건이 아니라, 이미 관측된 continuation behavior를 더 명확하게 다시 이름 붙인 것이다
- 현재 runtime이 이미 session summary와 parsed summary 데이터를 쓰고 있으므로, SQLite 계획은 작고 실용적으로 유지할 수 있다

## 현재 확인된 상태
현재 앱은 다음이 가능하다.
- Hanwha 카메라의 RTSP video 수신
- ONVIF/WiseAI metadata 수신
- raw metadata 및 parsed summary 저장
- 다수 객체 overlay 표시
- 실제 세션에서 `Car`, `Human`, `Bicycle` 감지 확인
- connection form, live frame view, evidence, metrics, recent metadata를 포함한 Qt verification shell 실행
- ad-hoc observation이 아니라 저장 세션 evidence 기반으로 현재 Hanwha 기준선을 설명 가능

## 아직 남은 공백
- Qt shell은 이제 사용 가능하지만, 임시 OpenCV view를 완전히 대체하려면 UI polish가 더 필요하다
- metadata 성능 해석은 아직 반복 detection 비중이 크고, 고유 객체 및 지속시간 관점이 더 필요하다
- fragmented payload 때문에 type label 오염이나 per-frame object completeness 문제가 여전히 남을 수 있다
- parser-health breakdown은 아직 로그와 분석 문서 중심이지, runtime UI에는 충분히 드러나지 않는다
- session review는 아직 로그 중심이며, SQLite 기반 조회는 정의만 되었고 구현은 시작되지 않았다

## 결정
현재 마일스톤은 기반 구축 및 observability 마일스톤으로 닫는다.

다음 마일스톤은 metadata evidence, parser health, metadata performance를 사용자가 런타임에서 바로 판단할 수 있게 만드는 방향으로 연다.

## 다음 마일스톤 목표
다음 마일스톤은 Qt shell을 주 검증 화면으로 끌어올리면서 아래 두 질문에 직접 답할 수 있어야 한다.
1. 지금 내가 보고 있는 객체에 대해 카메라가 실제 metadata를 보냈는가?
2. 세션 전체 기준으로 카메라 metadata 성능은 어느 정도인가?

이를 위해 다음이 필요하다.
- 읽기 쉬운 evidence, metrics, recent metadata 패널을 갖춘 Qt verification layout 정리
- 현재 message taxonomy를 바탕으로 한 operator-facing parser-health breakdown
- 객체 타입별 detection 수, unique object ID 수, 이후 object continuity / duration 관점으로 확장 가능한 세션 지표
- 반복 detection event와 고유 추적 객체를 구분하는 기준
- SungHwan이 로그를 직접 파지 않고도 metadata 품질을 평가할 수 있는 lightweight summary output
- 현재 runtime core를 유지한 채 SQLite 기반 session review를 준비하는 단계

## 권고
현재 아키텍처에서 방향을 틀 필요는 없다. 현재 C++/GStreamer runtime core를 유지하고, Qt shell을 새로운 presentation baseline으로 두고, 다음 마일스톤의 초점을 Qt polish, parser-health visibility, metadata performance reporting, SQLite 준비에 맞추는 것이 적절하다.

## 학습 관점에서의 의미
이번 마일스톤은 SungHwan에게 다음 학습 기준선도 제공했다.
- C++ desktop runtime이 config, session, parser, state, logging 책임으로 어떻게 나뉘는지
- full app과 control experiment를 분리해서 live system을 어떻게 디버깅하는지
- 카메라 동작, parser 동작, UI 동작을 구분해서 결론을 내려야 한다는 점
- 저장 세션 산출물을 evidence-backed baseline으로 바꾸고, 그것을 다시 concrete storage requirement로 연결하는 방법
