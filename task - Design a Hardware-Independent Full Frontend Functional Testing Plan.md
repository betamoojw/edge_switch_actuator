# Task — Design a Hardware-Independent Full Frontend Functional Testing Plan

Review, understand, and analyze the **`dev` branch of the current codebase**, with particular focus on the embedded frontend web interface, its backend/device-service dependencies, REST APIs, WebSocket/event communication, state management, configuration persistence, and hardware-dependent functionality.

The frontend web application is normally **hosted and served by the target device hardware**. The objective of this task is to determine how the complete frontend can be developed and functionally tested in a standard desktop web browser **without requiring the physical host device**.

## 1. Analyze the Existing Architecture

Inspect the actual `dev` branch implementation before proposing changes.

Identify and document:

- Frontend framework, build system, and development server configuration.
- How the frontend is built and embedded/served by the device firmware.
- All REST API endpoints consumed by the frontend.
- WebSocket connections, event names, message formats, and reconnection behavior.
- Request and response JSON structures.
- Authentication and authorization dependencies.
- Device configuration/state models.
- Persistent settings used by the frontend.
- Hardware-dependent frontend functionality.
- Relay/channel state interactions.
- Network/Wi-Fi/Ethernet state interactions.
- KNX-related configuration and runtime state, if implemented.
- Modbus-related configuration and runtime state, if implemented.
- Protocol selection and mutual-exclusion behavior, if applicable.
- OTA, system information, reboot, factory-reset, and other device-management functions exposed to the frontend.

Trace important functionality end-to-end:

```text
Frontend Component
        ↓
Frontend Service/API Client
        ↓
REST / WebSocket / Event
        ↓
Firmware Backend Handler
        ↓
Device State / Hardware / Protocol Service
```

Do not invent APIs or data structures. Derive them from the current implementation.

## 2. Extract the Frontend ↔ Device Contract

Create an inventory of the actual interface contract between the frontend and firmware.

For each interface, document where applicable:

```text
Endpoint/Event
HTTP Method
Request Payload
Response Payload
WebSocket/Event Payload
Authentication Requirement
Frontend Caller
Backend Handler
State Modified
Hardware Dependency
Error Behavior
```

Determine which interfaces can already operate without hardware and which require simulation.

The simulator design must be based on this extracted contract rather than creating an independent mock API.

## 3. Design Browser-Only Frontend Execution

Define how developers can launch and use the **real production frontend implementation** locally in a normal browser without an ESP32/host device.

Prefer an architecture similar to:

```text
Browser
   ↓
Frontend / Vite Development Server
   ↓
REST + WebSocket
   ↓
Device Simulator
```

while preserving the ability to switch to:

```text
Browser
   ↓
Frontend / Vite Development Server
   ↓
REST + WebSocket
   ↓
Real Device
```

The same frontend source and business logic should be used in both modes.

Avoid adding device-mocking logic directly into Svelte/UI components.

## 4. Evaluate and Design a Device Simulator

Determine whether a standalone **Device Simulator Service** is necessary.

If required, design it to reproduce the existing firmware's externally visible frontend interface as accurately as practical.

Prefer a stateful simulator rather than static JSON responses.

The simulator should potentially model, depending on functionality actually present in the current codebase:

```text
Device state
Relay/channel states
System information
Network state
Wi-Fi
Ethernet
Configuration persistence
Authentication
KNX state/configuration
KNX programming mode
KNX communication-object effects
Modbus configuration/state
Protocol selection
OTA state
Reboot
Factory reset
Errors/faults
```

A frontend request should modify simulator state and produce the same REST/WebSocket/event behavior expected from the actual firmware.

Also support **device-originated changes**, allowing the simulator to change state independently and push the corresponding event to the frontend.

## 5. Preserve API Fidelity

The primary design objective should be:

```text
From the frontend's perspective:

Real Device ≈ Device Simulator
```

Switching between them should require only configuration, for example:

```bash
npm run dev:sim
```

versus:

```bash
DEVICE_HOST=<device-ip> npm run dev:device
```

and should not require changes to frontend components or business logic.

Analyze the current Vite proxy configuration and propose how hard-coded device addresses should be replaced with configurable targets if necessary.

## 6. Simulator Scenarios and Fault Injection

Evaluate adding controllable simulation scenarios such as:

```text
Normal operation
Device booting
Device offline
Wi-Fi disconnected
Ethernet disconnected
Relay state changes
KNX programming mode
KNX communication failure
Modbus communication failure
Invalid configuration
Unauthorized request
REST error
REST timeout
WebSocket disconnect/reconnect
Factory-default state
OTA in progress
```

Where useful, include fault injection for:

```text
API latency
HTTP errors
Timeouts
WebSocket disconnection
Invalid payloads
Backend-originated state changes
```

These scenarios should enable testing UI behavior that is difficult to reproduce consistently with real hardware.

## 7. Simulator Control Interface

Evaluate whether a small simulator dashboard or control API should be provided.

It should allow developers/test automation to manipulate the virtual device, for example:

```text
Relay 1–N ON/OFF
Protocol selection
KNX programming mode
Simulated KNX telegram
Network connection state
Device reboot
Factory reset
WebSocket disconnect
REST failure
Artificial latency
```

Simulator-only interfaces must be clearly separated from the production device API.

## 8. Design Full Browser Functional Testing

Evaluate and preferably use **Playwright** for browser-level end-to-end testing.

Design tests covering all frontend functionality discovered during code analysis, including as applicable:

- Application startup and navigation.
- Authentication and permissions.
- Relay/channel control.
- Backend-originated relay changes.
- Network configuration.
- Wi-Fi configuration.
- Ethernet configuration.
- KNX configuration.
- KNX programming mode.
- KNX-originated state changes.
- Modbus RTU/TCP configuration.
- KNX/Modbus mutual exclusion.
- Settings load/save/reset.
- Configuration persistence.
- REST success/failure/timeout.
- WebSocket connection/reconnection.
- Device offline/online transitions.
- OTA UI.
- Reboot.
- Factory reset.
- Error handling.
- Loading/saving states.
- Browser refresh/state recovery.
- Responsive UI behavior.

Where practical, execute tests against Chromium, Firefox, and WebKit.

## 9. Establish Testing Levels

Design the development/testing strategy around three levels:

### Level 1 — Browser + Simulator

```text
Playwright
    ↓
Browser
    ↓
Real Frontend
    ↓
Device Simulator
```

No physical hardware dependency.

Suitable for local development and CI.

### Level 2 — Browser + Real Device

```text
Playwright
    ↓
Browser
    ↓
Real Frontend
    ↓
Real Device
```

Used to verify simulator/API fidelity and firmware integration.

### Level 3 — Hardware-in-the-Loop

```text
Browser
    ↓
Frontend
    ↓
Device Firmware
    ↓
Physical Hardware / KNX / Modbus / Network
```

Used for release and production verification.

Clearly define which tests belong to each level.

## 10. CI Integration

Design how Level-1 tests can run automatically without physical hardware.

For example:

```text
Checkout
   ↓
Install dependencies
   ↓
Build/check frontend
   ↓
Start Device Simulator
   ↓
Start frontend development/test server
   ↓
Run Playwright
   ↓
Collect logs/screenshots/traces
   ↓
PASS / FAIL
```

Define appropriate commands and CI requirements based on the actual project structure.

## 11. Implementation Constraints

The proposed solution must:

- Preserve current production behavior.
- Avoid unnecessary firmware changes.
- Avoid embedding mock logic in production UI components.
- Use the existing frontend API contract.
- Keep simulator-specific functionality isolated.
- Make simulator support optional.
- Keep the real-device development/testing path available.
- Avoid breaking the existing embedded frontend build process.
- Avoid breaking REST/WebSocket communication with real hardware.
- Prefer maintainable and modular implementation.
- Avoid duplicating device business rules where practical.
- Clearly identify any unavoidable differences between simulator and real hardware.

## 12. Produce an Implementation Roadmap

Break implementation into manageable phases, for example:

```text
Phase 1 — Architecture and API contract extraction
Phase 2 — Local browser execution
Phase 3 — Device simulator foundation
Phase 4 — REST API simulation
Phase 5 — WebSocket/event simulation
Phase 6 — Relay/device-state simulation
Phase 7 — KNX/Modbus/network simulation
Phase 8 — Scenario and fault injection
Phase 9 — Playwright functional tests
Phase 10 — CI integration
Phase 11 — Real-device contract verification
Phase 12 — HIL/release testing
```

For each phase identify:

- Objective.
- Files/modules affected.
- Proposed changes.
- Dependencies.
- Risks.
- Acceptance criteria.
- Tests required.

## 13. Deliverable

For this task, **do not immediately implement the complete simulator or modify production behavior**.

First perform the repository analysis and produce a detailed, codebase-specific implementation plan.

Save the result as a Markdown file in the repository, preferably:

```text
docs/FRONTEND_HARDWARE_INDEPENDENT_TEST_PLAN.md
```

If the repository does not currently use a `docs/` directory, choose an appropriate location consistent with the existing project structure.

The document should contain at minimum:

1. Current architecture analysis.
2. Frontend-to-device dependency map.
3. REST API inventory.
4. WebSocket/event inventory.
5. Hardware dependency analysis.
6. Proposed local browser development architecture.
7. Device Simulator architecture.
8. Simulator state model.
9. Simulation scenarios and fault injection.
10. Playwright/E2E testing architecture.
11. Complete frontend functional-test matrix.
12. CI integration strategy.
13. Real-device/HIL verification strategy.
14. Proposed file/directory changes.
15. Phased implementation roadmap.
16. Risks and mitigations.
17. Acceptance criteria.

Where useful, include Mermaid diagrams showing:

```text
Current architecture
Proposed simulator architecture
REST/WebSocket data flows
Frontend → backend → hardware dependencies
Three-level testing architecture
```

Base every recommendation on the **actual `dev` branch implementation**. Clearly distinguish between:

- Existing functionality confirmed in code.
- Functionality requiring modification.
- Proposed new simulator/test infrastructure.
- Optional future improvements.

The final plan should be sufficiently detailed that another coding agent can implement it phase-by-phase without needing to rediscover the architecture.