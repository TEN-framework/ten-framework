# Summary

There are three subgraphs, and two of them have the same version weight. The dependencies tree is as follows.

```bash
|- App
  |- ext_1@1.0.0
    |- ext_2@2.0.0
      |- ext_3@2.0.0
  |- ext_1@2.0.0
    |- ext_3@1.0.0
  |- ext_1@3.0.0
```

The inputs of clingo will be as follows.

```clingo
depends_on_declared("app", "1.0.0", "ext_1", "1.0.0").
depends_on_declared("app", "1.0.0", "ext_1", "2.0.0").

root("ext_1")
version_declared("ext_1", "3.0.0", 0).
version_declared("ext_1", "2.0.0", 1).
depends_on_declared("ext_1", "2.0.0", "ext_3", "1.0.0").
version_declared("ext_1", "1.0.0", 2).
depends_on_declared("ext_1", "1.0.0", "ext_2", "2.0.0").

version_declared("ext_2", "2.0.0", 0).
depends_on_declared("ext_2", "2.0.0", "ext_3", "2.0.0").

version_declared("ext_3", "2.0.0", 0).
version_declared("ext_3", "1.0.0", 1).
```

```bash
weight{app --> ext1@3.0.0} = 0
weight{app --> ext1@2.0.0 --> ext_3@1.0.0} = 1 + 1 = 2
weight{app --> ext1@1.0.0 --> ext@2.0.0 --> ext3@2.0.0} = 2 + 0 + 0 = 2
```

So the matched dependency chain will be `{app --> ext_1@3.0.0}`.
