import { readFileSync, writeFileSync } from 'node:fs';
const messages = {};
for (const line of readFileSync('src/lib/i18n/translations.tsv', 'utf8')
	.split(/\r?\n/)
	.filter(Boolean)) {
	const [key, ...values] = line.split('\t');
	if (values.length !== 6 || values.some((value) => !value.trim()))
		throw new Error(`Invalid translations: ${key}`);
	if (key in messages) throw new Error(`Duplicate translation: ${key}`);
	messages[key] = values;
}
writeFileSync('src/lib/i18n/messages.json', JSON.stringify(messages, null, '\t') + '\n');
console.log(`${Object.keys(messages).length} messages, six translated locales.`);
