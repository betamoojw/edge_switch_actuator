# Documentation publishing

The public site is
[betamoojw.github.io/edge_switch_actuator](https://betamoojw.github.io/edge_switch_actuator/).
Its source is `docs/` on **`dev`**, configured by `mkdocs.yml`. It documents the
development branch even though the repository's default branch is `main`.

## Preview and validate

From the repository root, in a Python environment:

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs serve
```

Open the localhost address printed by MkDocs. For the deployment build:

```sh
python -m mkdocs build --strict
```

Generated HTML is under `site/`, which is ignored by Git. Broken internal page,
file and anchor references are warnings and fail a strict build. External GitHub
source links are not network-validated by MkDocs; verify them when moving files.
Keep repository files outside `docs/` as explicit GitHub links, since they are
not copied into the site.

## Automatic publication

`.github/workflows/ci.yaml` is the **Documentation** workflow:

1. Documentation/configuration changes pushed to `dev` trigger a strict build.
2. Pull requests build for validation but do not deploy.
3. A successful `dev` build uploads only `site/` as a GitHub Pages artifact.
4. The deployment job uses the `github-pages` environment and the official Pages
   deployment action, with `pages: write` and `id-token: write` limited to that job.

Set repository **Settings → Pages → Build and deployment → Source** to
**GitHub Actions**. The `github-pages` environment must allow deployments from
`dev`; if it has a selected-branch rule, add `dev`. Do not select `main:/docs`:
these Markdown sources require MkDocs rendering. The old `gh-pages` branch is
not the source for this Actions-based workflow.

The workflow also supports `workflow_dispatch`. GitHub's manual Run workflow UI
requires the dispatchable workflow on the default branch; until `main` includes
this workflow, a documentation push to `dev` is the normal trigger. Failed runs
can be rerun from their run page.

After publication, check the workflow's deployment URL and verify the home page,
search, navigation and a nested page. A successful build alone does not prove
that Pages deployment or public serving succeeded.

Official references:
[GitHub custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
and [MkDocs deployment](https://www.mkdocs.org/user-guide/deploying-your-docs/).

## Updating the content

Use code as the source for current behavior. When reviewing a new firmware
baseline, update the source commit/version in the home page, README and review;
record the actual checks in `validation.md`. Preserve dates and limitations for
older hardware evidence. Add new user-facing pages to `nav` in `mkdocs.yml`.
Task records under `docs/tasks/` remain linked historical material, outside the
main navigation.

Do not place endpoint tokens, setup labels with real credentials, configuration
dumps or raw device secrets in the public documentation. This site publishes
every included file under `docs/`, not just pages shown in navigation.
