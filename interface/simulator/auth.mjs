import { createHmac, randomBytes, scryptSync, timingSafeEqual } from 'node:crypto';
export function hashPassword(password) {
	const salt = randomBytes(16).toString('hex');
	return `scrypt$${salt}$${scryptSync(password, salt, 32).toString('hex')}`;
}
export function verifyPassword(password, encoded) {
	if (typeof password !== 'string' || typeof encoded !== 'string') return false;
	const [, salt, hash] = encoded.split('$');
	if (!salt || !hash) return false;
	const actual = scryptSync(password, salt, 32);
	const expected = Buffer.from(hash, 'hex');
	return actual.length === expected.length && timingSafeEqual(actual, expected);
}
export class Auth {
	constructor(now) {
		this.now = now;
		this.rotate();
	}
	rotate() {
		this.secret = randomBytes(32);
		this.boot = randomBytes(8).toString('hex');
	}
	signature(value) {
		return createHmac('sha256', this.secret).update(value).digest('base64url');
	}
	token(user) {
		const head = Buffer.from(JSON.stringify({ alg: 'HS256', typ: 'JWT' })).toString('base64url');
		const data = Buffer.from(
			JSON.stringify({
				username: user.username,
				admin: user.admin,
				boot: this.boot,
				issued: this.now()
			})
		).toString('base64url');
		return `${head}.${data}.${this.signature(`${head}.${data}`)}`;
	}
	authenticate(token, users) {
		try {
			const [head, body, signature, extra] = token.split('.');
			const expected = Buffer.from(this.signature(`${head}.${body}`));
			const actual = Buffer.from(signature || '');
			if (extra || actual.length !== expected.length || !timingSafeEqual(expected, actual))
				return null;
			const p = JSON.parse(Buffer.from(body, 'base64url').toString());
			const age = this.now() - p.issued;
			return (
				users.find(
					(u) =>
						u.username === p.username &&
						u.admin === p.admin &&
						p.boot === this.boot &&
						Number.isFinite(age) &&
						age >= 0 &&
						age < 8 * 60 * 60 * 1000
				) || null
			);
		} catch {
			return null;
		}
	}
}
