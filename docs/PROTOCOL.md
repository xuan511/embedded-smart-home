# USB-UART line protocol / 两板通信协议

Only the Korvo USB-A Host → **CP2102N (VID 10C4, PID EA60)** USB-UART → STM32 USART1 path is implemented. UART is **115200, 8 data bits, no parity, 1 stop bit, no RTS/CTS**. UART TX/RX levels must be 3.3 V. This is an application protocol carried over UART; it does not require STM32 USB firmware.

## Framing and validation

- One ASCII line: `H1,TYPE,decimal_fields\n`. Receiver also accepts `\r\n`.
- Maximum **192 bytes including LF**, including CR if present. Encode buffer needs **193 bytes** to also hold a convenience NUL; NUL is never sent.
- Field counts are exact. No spaces, `+`, empty fields, hexadecimal, floating-point text, NaN, Infinity, extra fields, or integer overflow. Negative values are allowed by numeric syntax; semantic ranges below still apply. Leading decimal zeros are accepted.
- Half a line with no next byte for **500 ms** is discarded through the next LF. An overlong line, NUL or non-ASCII byte is also discarded through LF. Receiver then continues at the next line. Fragmented and joined USB transfers do not alter framing.
- `home_link.c` is the same parser/encoder compiled into both firmwares and the host tests. No dynamic allocation and no JSON/RPC framework.
- No extra checksum: USB already detects link transfer errors. UART framing/overrun errors discard the affected line, fields are range checked, and acknowledgements/session checks detect lost messages. This is a short desk-demo link, not a safety protocol; use short wires and test it on the actual bridge/board.

## Messages

All fields are signed 32-bit decimal numbers. `session` is **1..2147483647**. Request `seq` is **1..2147483647**; periodic telemetry uses 0. The Korvo sender uses one outstanding request at a time. Start a new session before sequence exhaustion. For boolean fields, 0 means false and 1 means true.

| Direction | Line body after `H1,` | Meaning |
| --- | --- | --- |
| Korvo → STM32 | `HELLO,session` | Establish a session; safe to repeat during handshake |
| STM32 → Korvo | `READY,session,stage` | Stage 1..4 identifies the compiled STM32 image |
| Korvo → STM32 | `CONFIG,session,seq,fan_on_c100,fan_off_c100,light_on_lux10,light_off_lux10,stale_ms` | Replace the whole runtime configuration atomically |
| Korvo → STM32 | `MODE,session,seq,automatic` | 0=manual, 1=automatic |
| Korvo → STM32 | `SET,session,seq,which,on` | 0=light, 1=fan. Switches the whole controller to manual |
| Korvo → STM32 | `PING,session,seq` | Heartbeat; does not restore outputs that timed out |
| STM32 → Korvo | `STATUS,session,seq,result,source,automatic,light,fan,temp_c100,humidity_pct100,lux10,valid_bits,sample_age_ms` | Current accepted controller/GPIO command state and sample. `seq>0` acknowledges a request; `seq=0` is periodic telemetry |

Units: temperature ÷100 = °C; humidity ÷100 = %RH; lux ÷10 = lux. Example 3000 means 30.00°C, 5500 means 55.00%RH, 500 means 50.0 lux. Threshold validity uses the existing `home_config_valid`: −40..125°C, `fan_off < fan_on`, 0..100000 lux, `light_on < light_off`, stale_ms **1000..60000**. Invalid complete configurations return result 1 without replacing the last accepted configuration.

`result`: **0=accepted, 1=invalid configuration, 2=CONFIG required for this session, 3=unsupported by compiled stage**. Frames with invalid syntax/session are silently discarded; caller times out. `source`: **0=no sample**, **1=STM32 TEST DATA**, **2=real sensor driver**. `valid_bits`: bit0 temperature, bit1 humidity, bit2 light. The STM32 clears bits for invalid or expired channels. Invalid channel numeric fields are 0; never treat the number without its validity bit as a reading. `sample_age_ms` is elapsed time from sample completion on STM32; unknown age saturates at 2147483647. Korvo must also add local elapsed time since receipt to decide freshness.

`light` and `fan` report STM32 accepted output commands. They are **not physical load feedback**. Stages 2/3 map light to the confirmed onboard LED and always report fan OFF; fan SET returns result 3. Stage 1 reports no sample and rejects MODE/SET. Stage 4 writes PB0/PB1; there is still no current or RPM feedback.

## Startup, save and reconnect

1. Korvo loads its NVS thresholds (or documented defaults after absent/invalid data). This is the persistent authority. STM32 boots with outputs OFF and remains unarmed until a valid CONFIG has been received.
2. Korvo chooses a fresh nonzero session and sends HELLO. Matching READY identifies the stage.
3. **CONFIG is sent before mode/load operations.** Only a matching STATUS with result 0 acknowledges synchronization. No configuration is copied from STM32 back into NVS.
4. Telemetry is sent about once per second. PING/STATUS form the basic bidirectional heartbeat. Changing a threshold on Korvo updates the desired configuration and synchronizes it; explicit save writes the Korvo NVS. Unsaved changes last until Korvo restarts.
5. On reconnect, send the current desired Korvo configuration, then wait for its acknowledgement. Discard old pending manual operations; do not replay them after reconnect. A **new-session HELLO immediately closes both outputs when STM32 is in MANUAL**, while keeping MANUAL mode. This also handles a broken STM32→Korvo wire: repeated new HELLO requests cannot keep an old manual ON output alive. AUTO retains valid local rules. STM32 keeps its runtime configuration while awaiting replacement CONFIG. Its **cold-boot mode is AUTO**.

STM32 rejects wrong-session commands. It remembers the highest sequence number and last result: same sequence receives an acknowledgement without reapplying the action; lower sequence is discarded. Repeating HELLO with the **same session** only replies READY and keeps the accepted configuration; a **new session** requires CONFIG again. Session separation protects against late bytes from an earlier USB connection.

In manual mode, no valid current-session request/heartbeat for **5000 ms** closes both demonstration outputs. Subsequent PING does not restore them; a fresh SET is necessary. In automatic mode, communication loss does not stop local sampling/rules. Invalid or expired temperature closes fan only; invalid/expired light closes light only. Humidity fault is displayed but has no control rule. Initial startup needs the first accepted CONFIG; subsequent disconnections preserve the last good runtime configuration.

## Minimal transcript

```text
Korvo  -> H1,HELLO,17
STM32  -> H1,READY,17,2
Korvo  -> H1,CONFIG,17,1,2800,2600,1000,2000,5000
STM32  -> H1,STATUS,17,1,0,1,1,1,0,3000,5500,500,7,0
Korvo  -> H1,SET,17,2,0,0
STM32  -> H1,STATUS,17,2,0,1,0,0,0,3000,5500,500,7,40
Korvo  -> H1,PING,17,3
STM32  -> H1,STATUS,17,3,0,1,0,0,0,3000,5500,500,7,60
```

Here 30°C/55%/50 lux are explicitly **test data** from the stage-2 firmware. Timing/values in this transcript are illustrative; it is not a captured hardware result. Run `python tools/test_host.py` for parser and controller tests. Follow [ACCEPTANCE.md](ACCEPTANCE.md) on physical boards before advancing stages.
