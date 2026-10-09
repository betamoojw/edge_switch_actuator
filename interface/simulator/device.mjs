import { mcpDefaults, mcpAction } from './xiaozhi-mcp.mjs';
import { EventEmitter } from 'node:events';
import { defaults, features, clone, integer, validateConfig, validateKnx } from './profiles.mjs';
import { Auth, hashPassword } from './auth.mjs';
import { readState, writeState } from './persistence.mjs';

export class Device extends EventEmitter {
	constructor({ profile = 'actuator', stateFile, clock = () => Date.now() } = {}) {
		super();
		this.clock = clock;
		this.offset = 0;
		this.stateFile = stateFile;
		this.auth = new Auth(() => this.now());
		this.saved = readState(stateFile);
		if (this.saved && this.saved.profile !== profile)
			throw new Error(
				'State file profile differs; use a separate state file or reset it explicitly'
			);
		if (!this.saved) this.seed(profile);
		this.saved.settings.xiaozhiMcpSettings ??= mcpDefaults();
		this.boot();
	}
	now() {
		return this.clock() + this.offset;
	}
	get features() {
		return features(this.saved.profile);
	}
	get actuator() {
		return this.saved.profile !== 'template';
	}
	seed(profile) {
		this.saved = defaults(profile);
		this.saved.users.forEach((u) => {
			u.password = hashPassword(u.password);
		});
		writeState(this.stateFile, this.saved);
	}
	reset(profile = this.saved.profile) {
		defaults(profile);
		this.seed(profile);
		this.boot();
	}
	boot() {
		this.auth.rotate();
		this.started = this.now();
		this.config = clone(this.saved.config);
		this.runtime = {
			network: {
				wifi: true,
				ethernet: false,
				ap: false,
				apClients: 0,
				ip: '192.0.2.10',
				interface: 'st1'
			},
			outputs: this.config.relays.map((r) => r.enabled && r.startup),
			blocks: Array(6).fill(false),
			sources: Array(6).fill('startup'),
			pulseUntil: Array(6).fill(0),
			programming: false,
			knxBusy: false,
			pressed: false,
			lastGesture: 0,
			gestureCount: 0,
			resetArmed: false,
			color: [0, 0, 0],
			manualColor: [255, 255, 255],
			manualBrightness: 10,
			rgbUntil: 0,
			toneHz: 0,
			toneUntil: 0,
			requests: 0,
			frameErrors: 0,
			rejected: 0,
			windowUntil: 0,
			windowPeer: 'rtu',
			tcpClients: 0,
			audit: [],
			fault: 0,
			error: '',
			scanUntil: 0,
			scanResults: [
				{
					ssid: 'Simulator WiFi',
					rssi: -45,
					bssid: '02:00:00:00:00:01',
					channel: 6,
					encryption_type: 3
				}
			],
			led_on: false,
			mqttConnected: false,
			mqttError: '',
			battery: { soc: 76, charging: false },
			coreDump: false,
			md5: '',
			ota: { status: 'none', progress: 0, error: '' },
			offlineUntil: 0,
			lastBus: this.now(),
			networkLost: 0
		};
		this.dedup = [];
		this.jobs = [];
		this.failures = {};
		this.startProtocol();
		this.tick();
		this.emit('boot');
	}
	schedule(ms, fn) {
		this.jobs.push({ at: this.now() + ms, fn });
	}
	reboot(ms = 1200) {
		this.boot();
		this.runtime.offlineUntil = this.now() + ms;
	}
	online() {
		const n = this.runtime.network;
		return !!(n.wifi || n.ethernet) && n.ip !== '0.0.0.0' && !!n.ip;
	}
	startProtocol() {
		this.runtime.programming = false;
		this.runtime.windowUntil = 0;
		this.runtime.tcpClients = 0;
		this.runtime.protocolState =
			this.config.mode === 'off'
				? 'off'
				: this.config.mode !== 'modbus_rtu' && !this.online()
					? 'waiting_network'
					: 'running';
		this.runtime.lastBus = this.now();
		if (this.config.mode === 'knx_ip' && this.saved.knx.configured) {
			this.saved.knx.parameters.forEach((p, i) => Object.assign(this.config.relays[i], p));
		}
		this.disableRelays();
	}
	disableRelays() {
		this.config.relays.forEach((r, i) => {
			if (!r.enabled) {
				this.runtime.outputs[i] = false;
				this.runtime.pulseUntil[i] = 0;
			}
		});
	}
	capabilities(user) {
		const configure = user.admin || user.role === 'installer';
		return {
			configure,
			command: configure || user.role === 'operator',
			admin: user.admin,
			channels: user.admin ? 63 : user.channels
		};
	}
	snapshot(user) {
		this.tick();
		const r = this.runtime;
		const n = r.network;
		const value = {
			revision: this.config.revision,
			mode: this.config.mode,
			protocolState: r.protocolState,
			fault: r.fault,
			error: r.error,
			sta: n.wifi,
			networkOnline: this.online(),
			networkInterface: this.online() ? n.interface : '',
			ip: this.online() ? n.ip : '0.0.0.0',
			ap: n.ap,
			apClients: n.apClients,
			uptime: Math.floor((this.now() - this.started) / 1000),
			relays: this.config.relays.map((c, i) => ({
				on: r.outputs[i],
				enabled: c.enabled,
				blocked: r.blocks[i],
				source: r.sources[i]
			})),
			programming: r.programming,
			knxConfigured: this.saved.knx.configured,
			pressed: r.pressed,
			lastGesture: r.lastGesture,
			gestureCount: r.gestureCount,
			resetArmed: r.resetArmed,
			color: r.color,
			toneHz: r.toneHz,
			rgbEnabled: this.config.rgb,
			buzzerEnabled: this.config.buzzer,
			buttonEnabled: this.config.button,
			rs485Enabled: this.config.rs485,
			requests: r.requests,
			frameErrors: r.frameErrors,
			rejected: r.rejected,
			configWindow: r.windowUntil > this.now(),
			tcpClients: r.tcpClients,
			audit: r.audit
		};
		if (user) value.capabilities = this.capabilities(user);
		return clone(value);
	}
	knxSnapshot(user) {
		const value = {
			...clone(this.saved.knx),
			active: this.runtime.programming,
			busy: this.runtime.knxBusy,
			developmentIdentity: true
		};
		if (user) value.capabilities = this.capabilities(user);
		return value;
	}
	commit(next) {
		if (this.failures.persistence) {
			delete this.failures.persistence;
			throw new Error('Configuration persistence failed');
		}
		writeState(this.stateFile, next);
		this.saved = clone(next);
	}
	audit(line) {
		this.runtime.audit.push(`${this.now() - this.started}: ${line}`);
		this.runtime.audit = this.runtime.audit.slice(-16);
	}
	relay(channel, value, source = 'web', pulse = false) {
		if (
			!integer(channel, 0, 5) ||
			!this.config.relays[channel].enabled ||
			this.runtime.blocks[channel]
		)
			return false;
		this.runtime.outputs[channel] = value;
		this.runtime.sources[channel] = source;
		this.runtime.pulseUntil[channel] = pulse ? this.now() + this.config.relays[channel].pulseMs : 0;
		return true;
	}
	tick() {
		const now = this.now();
		for (const job of this.jobs.filter((job) => job.at <= now)) {
			this.jobs = this.jobs.filter((x) => x !== job);
			job.fn();
		}
		const r = this.runtime;
		this.config.relays.forEach((cfg, i) => {
			if (r.pulseUntil[i] && r.pulseUntil[i] <= now) {
				r.outputs[i] = false;
				r.pulseUntil[i] = 0;
			}
			const lost =
				this.config.mode === 'modbus_rtu' && this.config.busWatchdog
					? now - r.lastBus > cfg.timeout * 1000
					: ['modbus_tcp', 'knx_ip'].includes(this.config.mode) &&
						r.networkLost &&
						now - r.networkLost > cfg.timeout * 1000;
			if (cfg.disconnectOff && lost && r.outputs[i]) {
				r.outputs[i] = false;
				r.sources[i] = 'disconnect policy';
			}
		});
		if (r.toneUntil <= now) r.toneHz = 0;
		const blink = now % 1000 < 500;
		const manual = r.rgbUntil > now && !r.fault && !r.programming && !r.resetArmed;
		const raw = r.resetArmed
			? [now % 200 < 100 ? 255 : 0, 0, 0]
			: r.fault
				? [blink ? 255 : 0, 0, 0]
				: r.programming
					? [255, 0, 0]
					: manual
						? r.manualColor
						: this.online()
							? [0, 255, 0]
							: r.network.ap
								? [0, r.network.apClients && blink ? 255 : 0, blink ? 255 : 0]
								: [blink ? 255 : 0, blink ? 100 : 0, 0];
		const brightness = this.config.rgb ? (manual ? r.manualBrightness : this.config.brightness) : 0;
		r.color = raw.map((value) => Math.floor((value * brightness) / 100));
	}
	advance(ms) {
		if (!integer(ms, 0, 86400000)) throw new Error('advanceMs must be 0–86400000');
		this.offset += ms;
		this.tick();
	}
	execute(path, body, user) {
		const cap = this.capabilities(user);
		if (!(path === 'device/commands' ? cap.command : cap.configure)) return { status: 403 };
		if (body.requestId?.length > 64) return { status: 422, body: { error: 'requestId too long' } };
		const signature = path + JSON.stringify(body);
		const prev = this.dedup.find(
			(d) => d.user === user.username && d.id === body.requestId && this.now() - d.at < 60000
		);
		if (body.requestId && prev)
			return prev.signature === signature
				? clone(prev.result)
				: { status: 409, body: { error: 'requestId reused with different payload' } };
		let status = 200,
			error = '';
		if (path === 'device/config' || path === 'protocol/transition') {
			const candidate =
				path === 'device/config'
					? clone(body)
					: { ...clone(this.config), mode: body.mode, revision: body.revision };
			delete candidate.capabilities;
			delete candidate.requestId;
			error = validateConfig(candidate);
			if (error) status = 422;
			else if (candidate.revision !== this.config.revision) {
				status = 409;
				error = 'Configuration revision conflict';
			} else if (
				this.config.mode === 'knx_ip' &&
				candidate.mode === 'knx_ip' &&
				this.saved.knx.configured &&
				candidate.relays.some((r, i) =>
					['enabled', 'startup', 'disconnectOff', 'timeout', 'pulseMs'].some(
						(k) => r[k] !== this.config.relays[i][k]
					)
				)
			) {
				status = 409;
				error = 'Edit commissioned relay parameters in the KNX section';
			} else if (this.failures.protocol) {
				delete this.failures.protocol;
				status = 409;
				error = 'Protocol failed; restored previous profile';
			} else {
				candidate.revision++;
				try {
					this.commit({ ...clone(this.saved), config: candidate });
					this.config = candidate;
					this.startProtocol();
					if (!candidate.rgb) this.runtime.rgbUntil = 0;
					if (!candidate.buzzer) this.runtime.toneUntil = 0;
				} catch {
					status = 409;
					error = 'Configuration persistence failed';
				}
			}
		} else if (path === 'knx/config') {
			error =
				this.config.mode !== 'knx_ip'
					? ''
					: validateKnx(body, this.knxSnapshot(), this.runtime.protocolState === 'running');
			if (this.config.mode !== 'knx_ip' || error) status = 409;
			else {
				try {
					const knx = {
						revision: this.saved.knx.revision + 1,
						address: body.address.split('.').map(Number).join('.'),
						owner: 'web',
						configured: true,
						objects: clone(body.objects),
						parameters: clone(body.parameters)
					};
					this.commit({ ...clone(this.saved), knx });
					knx.parameters.forEach((p, i) => Object.assign(this.config.relays[i], p));
					this.disableRelays();
				} catch {
					status = 409;
					error = 'KNX table commit failed; previous image restored';
				}
			}
		} else if (path === 'knx/programming') {
			if (this.config.mode !== 'knx_ip' || !this.online()) {
				status = 409;
				error = 'KNX requires an active IPv4 uplink';
			} else if (typeof body.active !== 'boolean') {
				status = 422;
				error = 'Expected active boolean';
			} else this.runtime.programming = body.active;
		} else if (path === 'device/commands') {
			const c = body.channel;
			const r = this.runtime;
			switch (body.command) {
				case 'relay':
				case 'pulse':
					if (!integer(c, 0, 5) || (body.command === 'relay' && typeof body.value !== 'boolean')) {
						status = 422;
						error = 'Invalid channel/value';
					} else if (!(cap.channels & (1 << c))) {
						status = 403;
						error = 'Channel permission denied';
					} else if (
						!this.relay(c, body.command === 'pulse' || body.value, 'web', body.command === 'pulse')
					) {
						status = 409;
						error = 'Channel disabled or blocked';
					}
					break;
				case 'all_on':
				case 'all_off':
					if (
						this.config.relays.some(
							(cfg, i) => cfg.enabled && (!(cap.channels & (1 << i)) || r.blocks[i])
						)
					) {
						status = 403;
						error = 'Bulk command denied';
					} else this.config.relays.forEach((_, i) => this.relay(i, body.command === 'all_on'));
					break;
				case 'rgb':
					if (
						!this.config.rgb ||
						['red', 'green', 'blue'].some((k) => !integer(body[k], 0, 255)) ||
						!integer(body.brightness ?? 10, 0, 100) ||
						!integer(body.seconds ?? 5, 1, 30)
					) {
						status = 422;
						error =
							'RGB requires enabled output, colors 0–255, brightness 0–100 and duration 1–30 seconds';
					} else {
						r.manualColor = [body.red, body.green, body.blue];
						r.manualBrightness = body.brightness ?? 10;
						r.rgbUntil = this.now() + (body.seconds ?? 5) * 1000;
					}
					break;
				case 'identify':
					r.manualColor = [255, 255, 255];
					r.rgbUntil = this.now() + 5000;
					break;
				case 'tone':
					if (
						!this.config.buzzer ||
						!integer(body.hz ?? 2000, 500, 4000) ||
						!integer(body.ms ?? 100, 10, 2000) ||
						!integer(body.duty ?? 25, 1, 50)
					) {
						status = 422;
						error = 'Tone requires enabled buzzer, 500–4000 Hz, 10–2000 ms and 1–50% duty';
					} else {
						r.toneHz = body.hz ?? 2000;
						r.toneUntil = this.now() + (body.ms ?? 100);
					}
					break;
				case 'acknowledge':
					r.toneHz = 0;
					r.toneUntil = 0;
					break;
				case 'unblock':
					if (!cap.configure) {
						status = 403;
						error = 'Unknown or unauthorized command';
					} else if (!integer(c, 0, 5)) {
						status = 422;
						error = 'Invalid channel';
					} else r.blocks[c] = false;
					break;
				case 'modbus_window':
					if (!cap.configure) {
						status = 403;
						error = 'Unknown or unauthorized command';
					} else if (!integer(body.seconds ?? 60, 0, 300)) {
						status = 422;
						error = 'Window must be 0–300 seconds';
					} else {
						r.windowUntil = this.now() + (body.seconds ?? 60) * 1000;
						r.windowPeer = body.peer ?? 'rtu';
					}
					break;
				case 'factory_reset':
					if (user.admin && body.confirm === 'ERASE') {
						this.schedule(50, () => {
							this.reset();
							this.runtime.offlineUntil = this.now() + 1200;
						});
						return { status: 200, body: { ok: true } };
					}
				// Deliberately use the same unauthorized response as unknown commands.
				default:
					status = 403;
					error = 'Unknown or unauthorized command';
			}
		}
		this.audit(`${user.username} /rest/${path} ${status}`);
		const result = { status, body: { ok: status === 200, error, revision: this.config.revision } };
		if (status === 200 && path === 'knx/config') {
			result.body.knx = this.knxSnapshot(user);
			result.body.revision = this.saved.knx.revision;
		}
		if (status === 200 && path === 'device/commands') result.body.state = this.snapshot();
		if (body.requestId) {
			this.dedup.push({
				user: user.username,
				id: body.requestId,
				signature,
				result: clone(result),
				at: this.now()
			});
			this.dedup = this.dedup.slice(-16);
		}
		return result;
	}
	action(body) {
		const r = this.runtime;
		switch (body.type) {
			case 'relay':
				if (
					typeof body.value !== 'boolean' ||
					!this.relay(body.channel, body.value, body.source || 'external', body.pulse === true)
				)
					throw new Error('Invalid, disabled or blocked channel');
				break;
			case 'network': {
				const wasOnline = this.online();
				const previous = JSON.stringify(r.network);
				for (const k of ['wifi', 'ethernet', 'ap', 'apClients', 'ip', 'interface'])
					if (body[k] !== undefined) r.network[k] = body[k];
				if (!this.features.ethernet) r.network.ethernet = false;
				if (wasOnline && !this.online()) r.networkLost = this.now();
				if (this.online()) r.networkLost = 0;
				if (
					previous !== JSON.stringify(r.network) &&
					['modbus_tcp', 'knx_ip'].includes(this.config.mode)
				)
					this.startProtocol();
				break;
			}
			case 'knx-object': {
				if (
					this.config.mode !== 'knx_ip' ||
					r.protocolState !== 'running' ||
					!this.saved.knx.configured ||
					!integer(body.number, 1, 18) ||
					typeof body.value !== 'boolean'
				)
					throw new Error('KNX object is unavailable');
				const channel = Math.floor((body.number - 1) / 3);
				if (body.number % 3 === 1) this.relay(channel, body.value, 'knx');
				if (body.number % 3 === 2) r.blocks[channel] = body.value;
				break;
			}
			case 'knx':
				if (body.busy !== undefined) r.knxBusy = !!body.busy;
				if (body.owner !== undefined) {
					if (!['ets', 'web', 'none'].includes(body.owner)) throw new Error('Invalid owner');
					this.commit({
						...clone(this.saved),
						knx: { ...clone(this.saved.knx), owner: body.owner }
					});
				}
				if (body.programming !== undefined && this.config.mode === 'knx_ip')
					r.programming = !!body.programming;
				break;
			case 'modbus':
				if (
					!['modbus_rtu', 'modbus_tcp'].includes(this.config.mode) ||
					r.protocolState !== 'running'
				)
					throw new Error('Modbus is unavailable');
				r.requests++;
				r.lastBus = this.now();
				if (body.frameError) r.frameErrors++;
				else if (
					body.channel !== undefined &&
					(!integer(body.channel, 0, 5) ||
						typeof body.value !== 'boolean' ||
						!(this.config.busMask & (1 << body.channel)) ||
						!this.relay(body.channel, body.value, 'modbus'))
				)
					r.rejected++;
				if (body.clients !== undefined) r.tcpClients = Math.max(0, Math.min(4, body.clients));
				break;
			case 'gesture': {
				if (body.pressed !== undefined) r.pressed = !!body.pressed;
				if (body.resetArmed !== undefined) r.resetArmed = !!body.resetArmed;
				if (body.holdMs >= 10000) {
					this.reset();
					break;
				}
				if (!integer(body.clicks, 1, 3)) break;
				r.lastGesture = body.clicks;
				r.gestureCount++;
				if (!this.config.button) break;
				const b = this.config.clicks[body.clicks - 1];
				for (let c = 0; c < 6; c++) {
					if (b.target && b.target !== c + 1) continue;
					if (b.action >= 1 && b.action <= 4)
						this.relay(
							c,
							b.action === 1 || b.action === 4 || (b.action === 3 && !r.outputs[c]),
							'button',
							b.action === 4
						);
				}
				if (b.action === 5) this.config.relays.forEach((_, i) => this.relay(i, false, 'button'));
				if (b.action === 6) {
					r.manualColor = [255, 255, 255];
					r.rgbUntil = this.now() + 5000;
				}
				if (b.action === 7) r.toneUntil = 0;
				if (b.action === 8 && this.config.mode === 'knx_ip') r.programming = !r.programming;
				break;
			}
			case 'reboot':
				this.reboot(body.durationMs ?? 1200);
				break;
			case 'factory-reset':
				this.reset();
				break;
			case 'offline':
				r.offlineUntil = body.active === false ? 0 : this.now() + (body.durationMs ?? 60000);
				break;
			case 'scan':
				if (!Array.isArray(body.networks)) throw new Error('networks array required');
				r.scanResults = clone(body.networks);
				break;
			case 'mcp':
				mcpAction(this, body);
				break;
			case 'mqtt':
				r.mqttConnected = !!body.connected;
				r.mqttError = body.error || '';
				break;
			case 'battery':
				r.battery = { soc: body.soc, charging: !!body.charging };
				break;
			case 'coredump':
				r.coreDump = !!body.available;
				break;
			case 'fault':
				r.fault = body.code || 0;
				r.error = body.error || '';
				break;
			case 'failure':
				if (!['persistence', 'protocol'].includes(body.target)) throw new Error('Unknown failure');
				this.failures[body.target] = true;
				break;
			case 'notification':
				this.emit('event', 'notification', {
					type: body.level || 'info',
					message: body.message || ''
				});
				break;
			case 'ota':
				this.ota(body.outcome || 'success');
				break;
			default:
				throw new Error('Unknown simulator action');
		}
		this.tick();
	}
	ota(outcome = 'success') {
		const publish = (value) => {
			this.runtime.ota = value;
			this.emit('event', 'otastatus', value);
		};
		publish({ status: 'preparing', progress: 0, bytes_written: 0, total_bytes: 0, error: '' });
		for (const progress of [20, 50, 80])
			this.schedule(progress * 25, () =>
				publish({
					status: 'progress',
					progress,
					bytes_written: progress * 1024,
					total_bytes: 102400
				})
			);
		this.schedule(2500, () => {
			publish(
				outcome === 'success'
					? { status: 'finished', progress: 100 }
					: { status: 'error', error: outcome }
			);
			if (outcome === 'success') this.schedule(300, () => this.reboot());
		});
	}
}
