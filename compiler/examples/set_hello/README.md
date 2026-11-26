# `.nwh` + binary `.set` example

`.nwh` files contain only prototypes. `nwc -h` compiles them into a binary
`NWSET001` archive; `link` checks that every prototype has an implementation
in the supplied `.nw` sources and emits one `.nwo`.

```bash
bash compiler/build.sh

cd compiler/examples/set_hello
../../builded/nwc -h hello.nwh -o headers.set
../../builded/link headers.set hello_impl.nw main.nw -o hello.nwo
```

`headers.set` stores the originating `hello.nwh` name, source line and column,
so a missing implementation is reported like this:

```text
[ ERROR ]: Unknown function name
headers.set | hello.nwh
3. 0A func_hello();
   ~~~~~~~~~~~
```

Several headers can go into one archive, and several archives can be linked at
once:

```bash
../../builded/nwc -h a.nwh b.nwh -o headers.set
../../builded/link headers.set extra.set impl.nw main.nw -o app.nwo
```

Run the regression tests with:

```bash
bash compiler/tests/test_nwh_set.sh
```
