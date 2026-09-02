# Using Python to configure your system

## Context

Sen uses a YAML file to fetch the info it needs to run a process. The YAML format is designed to be
easy to read and write by humans. It is also able to define complex data structures.

This is all nice and fun, until projects get bigger and configurations get more complicated. The
reality is that using a single file becomes increasingly challenging, as users want to re-use and
combine pieces of information from different places. The main driving reason is the proverbial
"single source of truth", and the chaos that might derive from an inconsistent or redundant
specification for our applications to run.

The natural consequence is that developers are "forced" to extend standard file formats (json,
yaml, xml, ini, etc.) to allow for "including" other files. And that works well for the first week
or so. The second week, people will start to realize that they don't need to just "include other
files" but "merge" them together, as maybe there are default parameters that need to be overwritten.
And, then we need to define some rules to define how the merging should work, which people need to
learn and developers need to implement (and test).

But the fun doesn't stop there, because now that we can include and merge files, we will be in the
need of doing things depending on some conditions (like: "include file X if this value is Y", or
"set this parameter to Z if this other parameter is W"). Moreover, we might want to fetch
environment variables, and even get them somehow overwritten by command-line arguments. Then, we
might want to add some rule-based system to make it more declarative, and maybe tweak the syntax of
the file format to allow for our custom needs. Don't forget about evaluating expressions like sums,
multiplications, concatenations, replacements, etc.

Do you see where we are going? Complex systems need powerful tools. A humble YAML or JSON file will
never be able to deal with all the needs we might have. We need some tooling on top. In some
systems, this tooling can get very sophisticated (full IDEs, with domain-specific knowledge and
static validation). But sometimes a programming language is more than enough to achieve these goals.

## Solution

By using Python scripts to hold our configuration, we automatically cover all the previous needs. We
just need to ensure that the configuration gets to Sen, and that it is valid.

The idea here is to keep YAML as the input format, but let Python generate the YAML for us. In order
to do that, we need to have the definition of the data containers that we need to fill in. The Sen
code generator helps us here by generating a set of
[Python dataclasses](https://realpython.com/python-data-classes/). We just fill those in as needed,
and tell Python to create the final YAML.

The YAML here is just an intermediate representation that you don't even need to read or keep under
version control. It can always be re-generated on demand.

Apart from all the power of Python (which is huge), you also get type safety (via
[mypy](https://mypy-lang.org/)), and all of this even before trying to run your Sen process.

## Example

Let's say that we have a set of types defined in an STL file:

```rust title="data.stl"
package my.data;

struct SomeStruct
{
  param1: string,
  param2: bool,
  param3: i32,
}

struct SomeOtherStruct
{
  a: f32,
  b: f32,
  c: f32,
}
```

Sen will generate a Python file with the following definitions:

```python title="data.py"
import yaml
from dataclasses import dataclass
from typing import Union, TypedDict, List, NewType
from datetime import datetime, timedelta
from enum import Enum

@dataclass
class SomeStruct:
    param1: str
    param2: bool
    param3: int

@dataclass
class SomeOtherStruct:
    a: float
    b: float
    c: float
```

Which you can now populate in your script(s). For example:

```python title="my_example.py"
from data import *

data1 = SomeStruct(
    param1='GenericDynamicModelImpl',
    param2=True,
    param3=4
)

data2 = SomeOtherStruct(
    a=2.3,
    b=3.4,
    c=4.5
)
```

Now write a script that generates the YAML. `asdict` turns the dataclasses into the plain
dictionaries that YAML can represent.

```python title="config.py"
import yaml
from dataclasses import asdict
from my_example import data1, data2

myObject = {
    'name': 'myObjectName',
    'class': 'my_package.MyClassImpl',
    'bus': 'local.tutorial',
    'prop1': asdict(data1),
    'prop2': asdict(data2),
}

shell = {
    'name': 'shell',
    'group': 2
}

ether = {
    'name': 'ether',
    'group': 3
}

myComponent = {
    'name': 'myComponent',
    'group': 4,
    'freqHz': 60,
    'imports': ['myPackage'],
    'objects': [myObject]
}

with open('config.yaml', 'w') as out:
    yaml.safe_dump({'load': [shell, ether], 'build': [myComponent]}, out, sort_keys=False)
```

Running that script writes the configuration Sen consumes:

```yaml title="config.yaml"
load:
- name: shell
  group: 2
- name: ether
  group: 3
build:
- name: myComponent
  group: 4
  freqHz: 60
  imports:
  - myPackage
  objects:
  - name: myObjectName
    class: my_package.MyClassImpl
    bus: local.tutorial
    prop1:
      param1: GenericDynamicModelImpl
      param2: true
      param3: 4
    prop2:
      a: 2.3
      b: 3.4
      c: 4.5
```
