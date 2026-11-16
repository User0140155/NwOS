# Contributing to NwOS
NwOS is an independent x86_64 operating system written in C and NASM. Contributions of all sizes are welcome — code, testing, documentation, bug reports, and ideas.

You do not need permission or prior approval to get started. If you see something worth improving, feel free to work on it and open a pull request.
Ways to contribute

There are several ways to help NwOS:
- Kernel: improve existing kernel components or add new functionality.
- Drivers: work on storage, input, USB, audio, networking, or other hardware support.
- Boot: improve the boot code and boot process.
- NwC: work on the compiler, runtime, language features, or examples.
- Applications: add or improve shell tools, games, and other programs.
- Testing: try NwOS in an emulator or on real hardware and report what works or fails.
- Documentation: improve explanations, examples, and build instructions.

Small fixes are useful too. You do not have to take on a large feature to make a contribution.
Getting started

1. Fork the repository.
2. Clone your fork.
3. Create a branch for your changes.
4. Make your changes.
5. Build the project and test what you changed.
6. Open a pull request describing the changes.

You can also open an issue if you find a bug or want to suggest a feature. This is optional; it is not a required step before contributing.
Project layout

Some of the main source directories are:
```bash
sys/
  boot/       Boot-related source files
  kernel/     Kernel source code

drivers/      Hardware drivers
compiler/     NwC compiler and related tools
NwDev/        NwOS development examples and tools
```
The repository may change as NwOS develops, so check the current source tree when looking for a particular component.
Building and testing
Use the project's Makefile and build scripts, following the instructions in the README, Follow instructions from the makefile (He uses quite a lot of compilers)

Before submitting a pull request:
- Build the project locally.
- Test the affected functionality where possible.
- Check that your changes do not introduce build errors.
- Include the test environment in your pull request (for example, QEMU or real hardware).
- If you cannot test something, say so clearly.

For driver changes, include the device or controller you tested with, if known. If a driver has only been tested in an emulator, mention that.
Pull requests

A useful pull request explains:
- What you changed.
- Why the change is needed.
- Which parts of NwOS are affected.
- How you built and tested it.
- Any limitations or things that still need work.

Keep pull requests focused when possible. Smaller changes are usually easier to review.

Please do not claim that hardware support works on all devices if you have only tested one configuration. Clear test information helps everyone.

## Code and project guidelines
- Follow the style used in the surrounding code.
- Keep changes related to the purpose of the pull request.
- Avoid removing existing copyright or license notices.
- Make sure new files use a license compatible with the project.
- NwOS is distributed under GPL-2.0-only. Read the LICENSE before contributing.
- Be clear about code or resources taken from other projects, and preserve their required notices and licenses.
You are free to use the tools that help you work, including AI-assisted tools. You are responsible for reviewing, understanding, building, and testing the code you submit.

## Bootloader development

The boot-related source files are in:
```text
sys/boot/
```
NwOS currently uses Limine for booting. Changes to the boot process should work with the project's existing setup; a separate bootloader is not needed for ordinary kernel contributions.

## Driver development

Driver source files are in:
```text
drivers/
```
Hardware support is an ongoing part of NwOS development. Contributions that improve existing drivers, add device support, or document hardware compatibility are welcome.

When possible, include:
- The device or controller model.
- Whether you tested on real hardware or in an emulator.
- Relevant logs or error messages.
- Steps to reproduce a problem.

## Community

Be respectful and constructive when discussing code and reviewing contributions. Different levels of experience are welcome.

Thanks for taking the time to work on NwOS