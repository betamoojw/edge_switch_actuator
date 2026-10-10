# Commissioning acceptance

These procedures are **not executed results**. They require the installation
owner's explicit authorization, an agreed maintenance window and a suitable
isolated fixture. The documentation inspection remained read-only.

Record board revision, firmware SHA-256, power source, fixture, instruments,
software versions, starting configuration and intended restoration state.
Have a qualified installer isolate operational loads and define the stop
condition before any output, reset, update or network-loss test.

| Test | Concrete procedure after authorization | Expected evidence |
| --- | --- | --- |
| Single relay and pulse | On one approved channel, command ON, OFF and one configured pulse; repeat independently for remaining channels | UI state plus measured COM/NO/NC response and pulse timing; restore each channel OFF or agreed state |
| Startup | Record policies; capture output pins/contacts through approved power cycle, reset and update restart | Initial electrical behavior and final configured states; software defaults alone are insufficient |
| Access boundaries | Use approved viewer/operator/installer accounts and a restricted channel mask; attempt approved allowed/denied operations | Denied operation leaves all affected outputs unchanged, including bulk preflight |
| Indicators and button | Record bindings; run one bounded RGB/tone test and each short gesture in an isolated fixture | Correct priority, limits, gesture/action and target; no long hold in a routine gesture test |
| Modbus | Select one transport with approval, run its linked acceptance plan, then restore previous mode | Read accuracy, atomic writes, exceptions, mask enforcement, physical output and transport timing |
| KNX | Agree addresses/ownership; commission, read back, send one controlled switch/block/status sequence | Correct group behavior and measured output; separate ETS import/download acceptance |
| HA and MCP | Enable on approved accounts/endpoints; read first, then one approved channel command; test reconnect without replay | Discovery/readiness, permissions, state propagation and no unintended output on reconnect |
| Disconnect policy | Record configuration; deliberately interrupt the selected uplink or RTU polling as applicable | Measured timeout behavior; MQTT/MCP loss alone does not add an OFF policy |
| OTA and persistence | Verify matching image; record private settings; perform manual OTA with local recovery available | Version/image identity, saved configuration, protocol recovery and output behavior |
| Destructive recovery | On a designated test unit only, approve reset and interruption tests separately | Settings erased/recovered as designed, setup identity retained, private evidence stored securely |

For detailed steps use [TCP acceptance](modbus-tcp-relay-fat-sat.md),
[RTU acceptance](modbus-rtu-relay-fat-sat.md) and the
[Modbus validation plan](MODBUS_VERIFICATION_VALIDATION_PLAN.md).
Stop on unexpected switching, heating, reset, loss of control or inconsistent
contact measurements. Restore the recorded configuration and verify the agreed
safe state before returning the installation to service.

Passing these functional procedures does not establish electrical certification,
load derating, EMC compliance, safety integrity or KNX certification. Those need
their own requirements, equipment, records and qualified assessment.
