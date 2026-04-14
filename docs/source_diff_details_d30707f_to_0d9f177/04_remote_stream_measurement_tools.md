# remote stream measurement tools 상세

이 문서는 M7에서 새로 추가된 `tools/remote_stream/*` helper 들이 어떤 검증 공백을 메우는지 정리한다.

## 디렉터리: `tools/remote_stream`

### 초기 코드

```text
(없음)
```

M6까지는 runtime 구현과 예제 payload 는 있었지만, 아래 항목을 반복 실행 가능한 도구 형태로 남겨 두지 않았다.

- MJPEG multipart header/sequence/timing 측정
- `/healthz` 장시간 샘플링과 `/proc/<pid>` drift 수집
- WebSocket `ready/error/ack` smoke transcript 저장
- orbit flood 와 latest-only queue 관찰

### 현재 코드

```text
tools/remote_stream/
  README.md
  measure_mjpeg.py
  collect_runtime_stats.py
  ws_control_smoke.mjs
  ws_pose_flood.js
  ws_pose_flood.mjs
```

#### `measure_mjpeg.py`

```python
parser.add_argument('--expect-control-sequence', type=int, default=-1, ...)
parser.add_argument('--control-send-unix-ms', type=int, default=0, ...)
parser.add_argument('--frames-csv', default='', ...)
parser.add_argument('--frames-jsonl', default='', ...)
```

```python
record = FrameRecord(
    ...
    control_sequence=control_sequence,
    capture_to_raw_ready_ms=capture_to_raw_ready_ms,
    encode_ms=encode_ms,
    capture_to_encoded_ms=capture_to_encoded_ms,
    encoded_unix_time_ms=encoded_unix_time_ms,
    receive_minus_encoded_ms=receive_minus_encoded_ms,
    ...
)
```

#### `collect_runtime_stats.py`

```python
parser.add_argument('--pid', type=int, default=0, ...)
parser.add_argument('--warmup-sec', type=float, default=0.0, ...)
parser.add_argument('--output-jsonl', required=True, ...)
parser.add_argument('--output-csv', default='', ...)
parser.add_argument('--summary-json', default='', ...)
```

```python
def read_proc_metrics(pid: int) -> dict[str, Any]:
    ...
    metrics['vmrss_kb'] = parse_kb('VmRSS')
    metrics['vmsize_kb'] = parse_kb('VmSize')
    metrics['vmhwm_kb'] = parse_kb('VmHWM')
    ...
```

#### `ws_control_smoke.mjs`

```js
transcript.push({ direction: "recv", payload: await waitForMessage(socket, options.timeoutMs) });
...
transcript.push({ direction: "send", file: path.relative(process.cwd(), sendFile), payload });
transcript.push({ direction: "recv", payload: await waitForMessage(socket, options.timeoutMs) });
```

#### `ws_pose_flood.mjs`

```js
summary.sentPayloads = index;
summary.ackCount = messages.filter((message) => message.parsed && message.parsed.type === 'ack').length;
summary.errorCount = messages.filter((message) => message.parsed && message.parsed.type === 'error').length;
summary.lastAck = [...messages].reverse().find((message) => message.parsed && message.parsed.type === 'ack')?.parsed ?? null;
if (args.healthzUrl) {
  summary.finalHealth = await fetchHealth(args.healthzUrl);
}
```

#### `README.md`

```md
- `measure_mjpeg.py`
  - Connects to `/stream.mjpg`
  - Parses multipart boundaries and per-frame headers
  - Can watch for a specific `X-Control-Sequence` to estimate control-to-visible timing
- `collect_runtime_stats.py`
  - Polls `/healthz`
  - Optionally samples `/proc/<pid>` for RSS/VmSize/thread/fd drift
```

### 바뀐 이유

- M7의 acceptance 는 “한 번 수동으로 봤다”가 아니라, **같은 실험을 다시 실행해서 숫자를 남길 수 있어야 한다**는 기준으로 잡았다.
- `measure_mjpeg.py` 는 MJPEG boundary/header 를 파싱하고 per-frame CSV/JSONL 을 남겨 single/two-client FPS, header timing, control-visible proxy 계산을 자동화한다.
- `collect_runtime_stats.py` 는 `/healthz` 와 `/proc` 를 함께 샘플링해 soak 동안의 queue/latency/RSS drift 를 한 번에 수집한다.
- `ws_control_smoke.mjs` 와 `ws_pose_flood.mjs` 는 브라우저 없이도 M6/M7 control contract 를 headless shell 에서 재현하기 위한 최소 client 이다.
- `README.md` 는 실제 M7 검증 명령을 그대로 적어, 이후 같은 Ubuntu host 에서 evidence 재수집이 가능하도록 만든다.

## 요약

- M7에서 추가된 tool 들은 제품 기능이 아니라 **verification harness** 다.
- 이 도구들이 있어야 verification report 의 수치를 같은 저장소 안에서 재현할 수 있고, 1-hour soak / clean SHA rerun 같은 후속 검증도 같은 형식으로 이어갈 수 있다.
