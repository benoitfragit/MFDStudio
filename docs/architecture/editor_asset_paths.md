# Editor Asset Path Defaults

`mfd_editor` centralizes asset-path defaults in `EditorAssetPathService`.

The service owns three related rules:

- default paths come from the repository source `assets/` tree unless the user
  starts the editor with `--asset-directory <path>`
- a configured asset directory is treated as the authored `assets` root, so
  `assets/windows/demo.json` maps to `<path>/windows/demo.json`
- safety checks for staged `_Exec` paths are shared by creation, deletion, and
  reference-scan workflows

The UI layer should not rebuild these paths itself. It should ask the service
for default asset locations and use native Windows file or folder pickers for
user selection.

For the new-window workflow:

- window JSON and page JSON fields use native save-file pickers
- the optional font field uses a native open-file picker
- the reticle-library field uses a native folder picker
- returning from a native picker reopens the creation popup so the draft state
  remains visible

The integrated tutorial uses the same path service. When `--asset-directory` is
provided, the tutorial window and page drafts are seeded under that configured
asset root instead of the repository `assets/` folder.
