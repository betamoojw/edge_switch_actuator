/** Prevent native form submission before running the application handler. */
export function preventDefault(handler: () => void) {
	return (event: SubmitEvent) => {
		event.preventDefault();
		handler();
	};
}
