# Documentation

Five documents, in the order they usually become relevant.

| Document | When it is what you want |
| --- | --- |
| [MFC.md](MFC.md) | The build stopped with `MSB8041: MFC libraries are required for this project`. Which component to install, in English and German, and the cases where it is installed and the error appears anyway. |
| [UI-UPGRADE.md](UI-UPGRADE.md) | The long one: what changed in the interface and why, the palette and its contrast ratios, the accelerator table, every static checker and the reasoning behind it, and the trade-offs that were deliberately not taken. |
| [TESTING.md](TESTING.md) | A build exists and someone has to press the buttons. What the program writes, how to back it up, and eight groups of checks. |
| [RELEASE.md](RELEASE.md) | Cutting a version: the six places the version number lives, the installer's `ProductCode` rule, building both platforms, publishing. |
| [workflows/README.md](workflows/README.md) | The two GitHub Actions files, why they live under `docs/` instead of `.github/workflows/`, and how to install them. |

Two more things worth knowing where to find:

- `tools/check-all.py` runs every checker with one verdict at the end;
  each checker explains itself in its own docstring.
- `../CHANGELOG.md` holds the release notes, and the entry for the current
  version is also the text of the GitHub release.

The screenshots under `img/` are **renderings** produced by
`tools/render-dialog.py` from the resource script, not photographs of a
running program. No machine in this project's history has compiled it yet.
