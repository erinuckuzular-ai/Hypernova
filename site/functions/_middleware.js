// Runs only on the paths listed in /_routes.json. The site folder is also the
// Pages output folder, so keep the function sources and deploy notes from being
// served as static files; everything else passes through.
const HIDDEN = [/^\/functions(\/|$)/, /^\/wrangler\.toml$/i, /^\/README\.md$/i];

export async function onRequest(context) {
  const { pathname } = new URL(context.request.url);
  if (HIDDEN.some((re) => re.test(pathname))) {
    return new Response("Not found", { status: 404, headers: { "Content-Type": "text/plain" } });
  }
  return context.next();
}
