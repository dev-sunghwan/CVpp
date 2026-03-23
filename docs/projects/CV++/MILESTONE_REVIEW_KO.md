# CV++ 마일스톤 정리

## 문서 정보
- 버전: `v0.1`
- 상태: `작성 완료`
- 작성일: `2026-03-23`
- 최종 수정일: `2026-03-23`
- 작성 주체: `PM / Tech Lead / Software Engineer Agents`

## 요약
현재 마일스톤은 기반 구축 마일스톤으로 성공적으로 마무리된 것으로 보는 것이 맞다. 팀은 실제 제품 목적을 분명히 했고, profile별 metadata 동작에 대한 잘못된 결론을 바로잡았으며, RTSP 제어 경로를 다음 개발이 가능할 정도로 안정화했고, 메인 앱에서 다중 객체 overlay를 다시 살렸다.

## 이번 마일스톤에서 달성한 것
- `config.toml` 기반 RTSP 설정 외부화
- `output/session-.../` 기반 세션 로그 구조 도입
- 세션별 raw metadata 및 parsed summary 저장
- 정상 종료 시 `PAUSE`, `TEARDOWN` 확인
- 카메라 동작 검증용 `metadata_probe` 도입
- `profile2`, `profile4` metadata 의미가 다르다는 기존 잘못된 결론 정정
- 메인 앱 metadata session이 선택한 auxiliary video pad를 실제 소비하도록 수정
- live 실행에서 다중 객체 파싱 및 다중 객체 overlay 복구

## 핵심 발견
- 문제의 핵심은 카메라 profile 자체가 아니었다
- 남아 있던 `gst-launch` 세션과 불완전한 probe가 잘못된 해석을 만들었다
- live metadata는 자주 fragment되므로 parser와 session 로직이 partial payload를 견뎌야 한다
- 메인 앱은 이제 여러 객체를 동시에 수신하고 화면에 표시할 수 있다
- 현재 로그의 객체 수는 고유 객체 수가 아니라 반복 detection event 수에 가깝다

## 현재 확인된 상태
현재 앱은 다음이 가능하다.
- Hanwha 카메라의 RTSP video 수신
- ONVIF/WiseAI metadata 수신
- raw metadata 및 parsed summary 저장
- 다수 객체 overlay 표시
- 실제 세션에서 `Car`, `Human`, `Bicycle` 감지 확인

## 아직 남은 공백
- evidence visibility가 아직 약하다. 사용자가 로그를 열지 않아도 metadata 수신 여부를 알아야 한다
- metadata 성능은 아직 반복 detection 기준에 가깝고, 고유 객체 기준 해석이 부족하다
- fragmented payload 때문에 type label 오염이나 per-frame object completeness 문제가 여전히 남을 수 있다

## 결정
현재 마일스톤은 기반 구축 및 observability 마일스톤으로 닫는다.

다음 마일스톤은 metadata evidence와 metadata performance를 사용자가 바로 판단할 수 있게 만드는 방향으로 연다.

## 다음 마일스톤 목표
다음 마일스톤은 아래 두 질문에 직접 답할 수 있어야 한다.
1. 지금 내가 보고 있는 객체에 대해 카메라가 실제 metadata를 보냈는가?
2. 세션 전체 기준으로 카메라 metadata 성능은 어느 정도인가?

이를 위해 다음이 필요하다.
- raw-seen, parsed-count, overlay-count, metadata age를 보여주는 evidence banner 또는 evidence panel
- 객체 타입별 detection 수와 unique object ID 수를 보여주는 세션 지표
- 반복 detection event와 고유 추적 객체를 구분하는 기준
- SungHwan이 로그를 직접 파지 않고도 metadata 품질을 평가할 수 있는 요약 출력

## 권고
현재 아키텍처를 버릴 필요는 없다. 지금의 메인 앱과 `metadata_probe` 기준선을 유지하고, 다음 마일스톤은 evidence visibility와 metadata performance reporting에 집중하는 것이 맞다.
