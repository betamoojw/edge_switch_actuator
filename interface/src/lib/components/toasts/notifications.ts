import { writable, derived, type Writable } from 'svelte/store';

type StateType = 'info' | 'success' | 'warning' | 'error';
type MessageParams = Record<string, string | number>;

type State = {
	id: string;
	type: StateType;
	message: string;
	params?: MessageParams;
};

function createNotificationStore() {
	const state: State[] = [];
	const notifications = writable(state);
	const { subscribe } = notifications;

	function send(
		message: string,
		type: StateType = 'info',
		timeout: number,
		params?: MessageParams
	) {
		const id = generateId();
		setTimeout(() => {
			notifications.update((state) => {
				return state.filter((n) => n.id !== id);
			});
		}, timeout);
		notifications.update((state) => {
			return [...state, { id, type, message, params }];
		});
	}

	return {
		subscribe,
		send,
		error: (msg: string, timeout: number, params?: MessageParams) =>
			send(msg, 'error', timeout, params),
		warning: (msg: string, timeout: number, params?: MessageParams) =>
			send(msg, 'warning', timeout, params),
		info: (msg: string, timeout: number, params?: MessageParams) =>
			send(msg, 'info', timeout, params),
		success: (msg: string, timeout: number, params?: MessageParams) =>
			send(msg, 'success', timeout, params)
	};
}

function generateId() {
	return '_' + Math.random().toString(36).substr(2, 9);
}

export const notifications = createNotificationStore();
