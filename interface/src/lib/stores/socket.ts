import { writable } from 'svelte/store';
import msgpack from 'msgpack-lite';

export function createWebSocket() {
	const listeners = new Map<string, Set<(data?: any) => void>>();
	const { subscribe, set } = writable(false);
	const localEvents = new Set([
		'open',
		'close',
		'error',
		'message',
		'binary',
		'json',
		'unresponsive',
		'pong'
	]);
	let ws: WebSocket | undefined;
	let socketUrl: string | URL;
	let useJson = false;
	let stopped = true;
	let retry = 1000;
	let lossReported = false;
	let reconnectTimer: ReturnType<typeof setTimeout> | undefined;
	let heartbeatTimer: ReturnType<typeof setTimeout> | undefined;
	let pongTimer: ReturnType<typeof setTimeout> | undefined;
	const emit = (event: string, data?: unknown) => listeners.get(event)?.forEach((fn) => fn(data));

	function clearTimers() {
		clearTimeout(reconnectTimer);
		clearTimeout(heartbeatTimer);
		clearTimeout(pongTimer);
	}
	function stop() {
		stopped = true;
		clearTimers();
		const old = ws;
		ws = undefined;
		old?.close();
		set(false);
	}
	function send(data: unknown) {
		if (ws?.readyState === WebSocket.OPEN)
			ws.send(useJson ? JSON.stringify(data) : msgpack.encode(data));
	}
	function sendEvent(event: string, data: unknown = {}) {
		send({ event, data });
	}
	function disconnect(current: WebSocket, reason: string, event?: unknown) {
		if (current !== ws || stopped) return;
		ws = undefined; // Ignore the subsequent close/error callbacks from this socket.
		clearTimers();
		current.close();
		set(false);
		if (reason !== 'close') emit(reason, event);
		if (!lossReported) emit('close', event);
		lossReported = true;
		reconnectTimer = setTimeout(connect, retry);
		retry = Math.min(retry * 2, 30000);
	}
	function heartbeat(current: WebSocket) {
		if (current !== ws || stopped) return;
		sendEvent('ping');
		pongTimer = setTimeout(() => disconnect(current, 'unresponsive'), 30000);
	}
	function connect() {
		if (stopped) return;
		const current = new WebSocket(socketUrl);
		ws = current;
		current.binaryType = 'arraybuffer';
		current.onopen = (event) => {
			if (current !== ws) return;
			set(true);
			// A reconnect is confirmed by a valid frame, not merely a TCP handshake.
			if (!lossReported) emit('open', event);
			for (const name of listeners.keys()) if (!localEvents.has(name)) sendEvent('subscribe', name);
			heartbeatTimer = setTimeout(() => heartbeat(current), 15000);
		};
		current.onmessage = (message) => {
			if (current !== ws) return;
			const binary = message.data instanceof ArrayBuffer;
			emit(binary ? 'binary' : 'message', message.data);
			let payload;
			try {
				payload = binary ? msgpack.decode(new Uint8Array(message.data)) : JSON.parse(message.data);
				if (!payload || typeof payload !== 'object') throw new Error('Invalid event frame');
			} catch (error) {
				emit('error', error);
				return;
			}
			if (payload.event === 'pong') {
				retry = 1000;
				clearTimeout(pongTimer);
				clearTimeout(heartbeatTimer);
				heartbeatTimer = setTimeout(() => heartbeat(current), 15000);
			}
			if (lossReported) {
				lossReported = false;
				emit('open');
			}
			emit('json', payload);
			if (payload.event) emit(payload.event, payload.data);
		};
		current.onerror = (event) => disconnect(current, 'error', event);
		current.onclose = (event) => disconnect(current, 'close', event);
	}
	function init(url: string | URL, use_json = false) {
		if (!stopped && String(url) === String(socketUrl) && useJson === use_json) return;
		stop();
		socketUrl = url;
		useJson = use_json;
		stopped = false;
		lossReported = false;
		retry = 1000;
		connect();
	}
	function off(event: string, listener?: (data: any) => void) {
		const members = listeners.get(event);
		if (!members) return;
		if (listener) members.delete(listener);
		else members.clear();
		if (!members.size) {
			listeners.delete(event);
			if (!localEvents.has(event)) sendEvent('unsubscribe', event);
		}
	}
	return {
		subscribe,
		init,
		stop,
		send,
		sendEvent,
		off,
		on<T>(event: string, listener: (data: T) => void): () => void {
			let members = listeners.get(event);
			const first = !members;
			if (!members) {
				members = new Set();
				listeners.set(event, members);
			}
			members.add(listener);
			if (first && !localEvents.has(event)) sendEvent('subscribe', event);
			return () => off(event, listener);
		}
	};
}
export const socket = createWebSocket();
