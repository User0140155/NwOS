# nw Package manager
We gonna make a package manager that updates NwOS, or install local packages

NwOS folder with installed programs is: home/pkg/

And their source code located in local/ (In real source code OS)

## Commands

```bash
nw install <pkg>    - Install file (local)
nw update            - Update system to the last version
nw remove  <pkg>     - remove package
```
## Local repository

Package sources live in `local/`. Every package is either:

- a flat `local/<name>.nw` file, or
- a directory `local/<name>/` with a `package.conf`, its `.nw` source,
  and an optional `main.nw`.

`tools/gen_packages.py` scans `local/` during `build.sh` and embeds the
repository into the kernel as `sys/kernel/packages_data.h`. The script
validates package names and the NwC source-size limit (16 KiB).

Example `local/hello/package.conf`:

```ini
NAME=hello
VERSION=1.0
DESCRIPTION=Test package installed from local repo
COMMAND=hello
SOURCE=main.nw
```

If `package.conf` is omitted, the directory name becomes the package
name and `main.nw`/`<name>.nw` is used as the source.

## Installed packages

`nw install <pkg>` compiles the embedded NwC source with the in-kernel
compiler and writes `/home/pkg/<pkg>.nwo`. The package is then available
as a short shell command (`hello` runs `/home/pkg/hello.nwo`), and also
via `nw run <pkg>`.

`nw update` rebuilds every installed package from the repository that is
embedded in the currently booted NwOS image.

## Full command list

```bash
nw list              - list packages in the local repository
nw install <pkg>     - compile and install a package
nw remove <pkg>      - remove /home/pkg/<pkg>.nwo
nw update            - rebuild all installed packages
nw run <pkg>         - run an installed package
nw --help            - show package manager help
```

Aliases kept for the original syntax: `nw -S <pkg>`, `nw -R <pkg>`,
and `nw -lc`.
