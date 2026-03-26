# CV++ ONVIF 메타데이터 파이프라인 조사

## 문서 정보
- 버전: `v0.2`
- 상태: `profile4/profile2 full app smoke 검증 완료; startup 안정성 개선 진행 중`
- 작성일: `2026-03-25`
- 최종 수정일: `2026-03-26`
- 작성 주체: `Software Engineer Agent`

## 목적
이 문서는 CV++ 메타데이터 파이프라인이 raw RTP heuristic 경로에서 ONVIF-aware transport 경로로 어떻게 좁혀졌는지, 그리고 최신 full-app 검증 기준으로 profile 일관성과 startup 안정성이 어디까지 확인되었는지를 기록한다.

## 왜 이 조사가 필요했는가
저장된 세션에서는 아래와 같은 현상이 반복해서 보였다.
- `malformed-payload`
- `XML start marker not found`
- partial object recovery sequence
- `Caur`, `Caar`, `Cayr`, `Hukuman` 같은 parser-noise label

그래서 다음 질문이 생겼다.
- 카메라가 원래 깨진 object label을 보내는가?
- 아니면 앱이 RTP-framed ONVIF metadata를 너무 이른 단계에서 읽어서 transport chunk를 complete XML document처럼 다루고 있는가?

## 초기 파이프라인의 문제
이전 메타데이터 경로는 metadata RTP pad를 곧바로 `appsink`에 연결했다.

저장 세션에서 관측된 metadata pad caps:
- `application/x-rtp`
- `media=application`
- `encoding-name=VND.ONVIF.METADATA`

해석:
- 앱은 이미 재조립된 ONVIF metadata document를 받는 구조가 아니었다
- RTP-framed ONVIF metadata sample을 직접 받고 있었다
- 이후 앱은 `<?xml`을 찾고, 뒤 chunk를 `pending_xml_fragment_`에 이어붙이는 방식으로 XML 경계를 추정했다

즉, 이전 구조는 metadata-aware RTP depayload 경로가 아니라, 애플리케이션 레벨 XML-boundary heuristic에 의존하고 있었다.

## ONVIF 방향 점검
ONVIF 방향 자체는 맞다.

로컬 환경 확인 결과:
- `rtponvifmetadatadepay` 사용 가능
- sink caps는 `application/x-rtp` + `encoding-name=VND.ONVIF.METADATA`를 받는다
- source caps는 `application/x-onvif-metadata`를 낸다

이 방향은 ONVIF RTP metadata 처리 방식과 맞다.

## Probe 비교
비교 조건:
- 같은 카메라
- 같은 `profile4`
- 같은 probe duration: `20s`

### 기존 raw-RTP probe 경로
경로 의미:
- raw RTP metadata sample을 `appsink`로 직접 전달

관측된 appsink caps:
- `application/x-rtp`

관측된 요약:
- `metadata_samples=406`
- `object_payloads=398`
- `event_only=8`
- `malformed=200`

전형적 동작:
- `malformed-payload`와 `success`가 반복적으로 교차함
- 어떤 object는 malformed 단계에서는 `Vehicle`로 나오고, recovery 뒤에는 `Car`로 바뀌는 식의 partial parse가 반복됨

### RTP-aware ONVIF probe 경로
경로 의미:
- `rtpjitterbuffer -> rtponvifmetadatadepay -> appsink`

관측된 appsink caps:
- `application/x-onvif-metadata`

관측된 요약:
- `metadata_samples=206`
- `object_payloads=198`
- `event_only=8`
- `malformed=0`

전형적 동작:
- object-bearing payload가 바로 `success`로 들어옴
- 이전 probe에서 보이던 malformed/recovery 교차 패턴이 사라짐

## 현재 해석
현재 증거는 다음 결론을 지지한다.
- 이전 fragmentation 동작의 상당 부분은 카메라만의 문제가 아니라 CV++ metadata capture path에서 온 것이다
- 이전 앱 경로는 RTP-framed ONVIF metadata를 너무 이른 단계에서 읽고 있었다
- metadata-aware ONVIF depayload path를 쓰면 appsink 경계가 RTP packet이 아니라 ONVIF metadata buffer로 이동한다
- 이 변경만으로도 관측된 비교에서 probe 기준 malformed count가 `200`에서 `0`으로 떨어졌다

## 무엇이 개선되었는가
분명히 좋아진 점:
- appsink가 `application/x-rtp` 대신 `application/x-onvif-metadata`를 받기 시작했다
- probe 비교에서 malformed payload 비율이 크게 떨어졌다
- object-bearing payload가 complete parseable metadata로 더 자주 들어왔다
- 애플리케이션 레벨 recovery heuristic 의존도가 줄었다

## Full-App 검증: Profile4
이번에 사용한 fresh full-app 세션:
- 세션: `output/session-20260325-160520/`
- 대상: `profile4`
- session.log에 기록된 runtime target: `rtsp://.../profile4/media.smp`
- session.log에 기록된 metadata path: `RTP jitterbuffer + ONVIF metadata depay path`
- session.log에 기록된 first metadata appsink caps: `application/x-onvif-metadata`

관측된 parsed summary 분포:
- `success=19847`
- `no-objects=287`
- `malformed-payload=0`
- `noise_detections=0`

해석:
- 개선된 ONVIF path는 probe에서만 성공한 것이 아니다
- full app도 metadata가 실제로 흐르기 시작한 뒤에는 clean object-bearing parse path에 도달했다
- 검증된 `profile4` full-app run에서는 이전 parser-noise label이 나타나지 않았다

## 더 좁혀진 원인
이번 추가 검증으로 원인이 한 단계 더 좁혀졌다.
- ONVIF depay path 자체는 맞았다
- full-app failure mode 일부는 `MetadataRtspSession` 안에서 metadata와 auxiliary video를 함께 선택하던 구조와 관련이 있었다
- 앱 metadata session을 metadata-only selection으로 바꾼 뒤에는 full app도 probe와 같은 방향으로 개선되었다: appsink는 `application/x-onvif-metadata`를 받고, parse 결과는 clean하게 들어왔다

## Startup 안정화 작업
남은 문제는 transport correctness보다 startup timing이었다.

이번에 넣은 조정:
- video first frame 직후 metadata를 바로 시작하지 않고, 잠깐 안정화 시간을 둠
- metadata watchdog를 단순 세션 시작 기준이 아니라 metadata pad-link timing과 post-link sample timing 기준으로 더 정교하게 봄

의도:
- metadata pad가 막 연결되려는 순간을 failure로 오판하지 않기
- overlay가 늦게 붙는 현상을 줄이기

## Full-App Smoke 검증: Profile2
초기 profile2 smoke에서는:
- first video sample은 도착했지만
- metadata pad link가 watchdog threshold 전에 오지 않아서 retry가 1번 필요했다
- 다만 retry 뒤에는 `application/x-onvif-metadata`와 clean parse path로 들어왔다

startup 안정화 조정 후 fresh profile2 smoke 세션:
- 세션: `.tmp_profile2_smoke/output/session-20260326-001245/`
- `VideoSession: first video sample received: 1920x1080`
- `MetadataSession: starting pipeline`
- `MetadataSession: linked metadata RTP pad to jitterbuffer`
- `MetadataSession: first appsink sample caps=application/x-onvif-metadata`
- `MetadataSession: first metadata sample received`
- metadata watchdog retry 없음

관측된 parsed summary 분포:
- `success=28`
- `no-objects=16`
- `malformed-payload=0`

해석:
- `profile2`는 다른 metadata 의미 모델이 필요한 것이 아니다
- `profile4`와 같은 ONVIF-aware 파이프라인 규칙으로 처리할 수 있다
- 남은 차이는 metadata semantics가 아니라 startup timing 민감도였다
- 그 startup timing도 이번 smoke 기준으로는 개선된 방향이 확인됐다

## 갱신된 실무 결론
현재 metadata transport 방향은 검증 완료로 봐도 된다.
- 이전 fragmentation과 type-label 오염은 기존 app-side RTP handling path의 영향을 크게 받았다
- ONVIF-aware depayload path가 올바른 runtime 방향이다
- metadata session을 metadata-only로 두는 것이 full app에서 probe 수준 결과를 재현하는 데 필요했다
- `profile2`와 `profile4`는 metadata 의미 차이가 아니라 startup 안정성 관점에서만 다뤄야 한다

## 아직 남은 후속 작업
다음 항목은 아직 남아 있다.
- 주간 object-rich scene에서 overlay responsiveness 최종 검증
- 비시각 smoke verification 절차를 작은 반복 가능한 방식으로 정리
- video session 쪽 startup watchdog 추가 안정화
- 여러 반복 실행에서도 같은 startup behavior가 유지되는지 확인

## 권고
ONVIF-aware metadata pipeline을 현재 기준 baseline으로 확정하고, 다음 작업은 startup 안정성 반복성, smoke verification 절차 정리, 그리고 주간 operator-facing 검증으로 이어가는 것이 맞다.
