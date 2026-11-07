# Contributing

## To contribute

Before contributing:

1. Contact the creator, User0140155 (TikTok: @csplayer421), or open an issue.
2. Provide the following information:
   - Why you want to contribute.
   - What you plan to work on (kernel, bootloader, drivers, etc.).
   - Your GitHub username.
3. Read the license if you haven't already.

And that's it!

## Developing the kernel

You can use AI tools or write the code yourself.

The main kernel files are located in:

```text
sys/kernel/
```

Build the project using the Makefile.

**IMPORTANT:** Do not submit a pull request unless you have compiled the project yourself. If your changes cause build errors, fix them before submitting a pull request.

## Developing the bootloader

You can use AI tools or write the code yourself.

The main bootloader files are located in:

```text
sys/boot/
```

**Note:** Working on the kernel is probably a better choice for now. The OS already boots using Limine, so there is no need to create a separate `boot.asm`.

## Developing drivers

You can use AI tools or write the code yourself.

The driver files are located in:

```text
drivers/
```

We need more driver developers because our drivers do not all work on real hardware yet.

## Other

Knowing Russian is a plus, but you can still communicate in English.