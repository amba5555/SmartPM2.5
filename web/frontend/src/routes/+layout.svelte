<svelte:head>
	<title>Smart PM2.5 Dashboard</title>
	<link rel="preconnect" href="https://fonts.googleapis.com">
	<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
	<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700;800&display=swap" rel="stylesheet">
</svelte:head>

<style>
	:global(html, body) {
		font-family: 'Inter', system-ui, -apple-system, 'Segoe UI', Roboto, 'Helvetica Neue', Arial;
		margin: 0;
		padding: 0;
		background: #ffffff;
		color: #111827;
	}
</style>

<slot />
<script>
	// Accept SvelteKit route props to prevent warnings about unknown props
	// This page doesn't use route params; export a const to silence the Svelte plugin warning
	export const params = undefined;
</script>