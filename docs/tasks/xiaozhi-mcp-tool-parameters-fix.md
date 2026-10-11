# Xiaozhi status tool parameter compatibility — 2026-10-10

The user reported `Invalid tool parameters` for
`actuator_status_ccba9734d9ac`. The exact cloud request was not captured.

## Reproduced defect

`Protocol::receive()` previously required exactly two `tools/call.params`
members and a mandatory object-valued `arguments`. A valid argument-free
status request therefore failed before dispatch:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": { "name": "actuator_status_ccba9734d9ac" }
}
```

The negotiated [MCP 2024-11-05 schema](https://github.com/modelcontextprotocol/modelcontextprotocol/blob/main/schema/2024-11-05/schema.ts)
defines `CallToolRequest.params.arguments` as optional and permits request
metadata in `params._meta`. Including metadata also broke the previous
exact-member-count check, even when tool arguments were valid. These are
confirmed compatibility defects consistent with the reported tool and error;
the user's precise wire representation is still unverified.

## Fix and tests

- Accept optional `arguments`, normalizing an omitted value to an empty object.
- Accept object-valued `_meta` without forwarding it as tool input. Do not
  constrain the request envelope's member count.
- Continue rejecting explicit null, array or string arguments, malformed
  metadata, missing tool names, unexpected actual tool arguments, missing relay
  or pulse inputs and string-valued booleans. Channel permissions are unchanged.
- Preserve duplicate-call protection and the per-tool schema checks.

The new native regression failed on the original parser's first argument-free
status call. After the fix, `python scripts/test_xiaozhi_mcp.py` passed the
protocol/settings, transport, lifecycle and feature-guard suites. Coverage
includes status with omitted/empty arguments, metadata-bearing calls,
argument-free alias/all-off/identify, metadata-bearing relay calls, duplicate
suppression and malformed calls rejected before execution. All actuator writes
in these tests use a fake device; no physical relay command is sent.

The user should retry the same status tool through Xiaozhi after deployment to
confirm the actual cloud-client flow. A cloud `Ready` state alone does not prove
that the agent has invoked the tool successfully.

## Deployment

Built `waveshare-relay-6ch` successfully and passed the binary-size guard:
2,160,912 / 3,342,336 bytes. Application SHA-256:
`83550f94e1215e223f44955f4ce8178e48845486e86754b4065541df9028d78a`.
Version remains `0.6.2`.

OTA to `device.example` (MAC `CC:BA:97:34:D9:AC`) returned HTTP 200. After the
expected OTA reboot, actuator and KNX configuration matched the pre-update
snapshots; KNX was running with fault 0 and all relays OFF. All 11 embedded UI
assets matched the build. MCP reached `ready` with six tools; a subsequent
snapshot showed uptime 36 seconds and 5116 bytes of HTTP minimum free stack.
The user's current alias was retained. No physical tool command was issued.

Local build evidence: `.pio/mcp-tool-params-build.log`. OTA and post-boot
evidence: `.pio/ota-mcp-tool-params/`. Real Xiaozhi agent retry remains pending.
