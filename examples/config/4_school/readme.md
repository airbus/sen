# School example

This is a small example that illustrates how objects can discover and interact with each other.

The idea is:

- A classroom has one teacher and multiple students. The Classroom object can automatically
  instantiate a teacher if the `createTeacher` flag is set to `true`.
- Students can be added/removed by calling `addStudents`/`removeStudents`. They will be published to
  the `studentsBus` bus.
- When a student makes a noise (by emitting the `madeSomeNoise` event, some other students will get
  called to the `hearSomeNoise` method). This is also done in the `Classroom`.
- The teacher discover the students and interacts with them.
- Everyone has a "state" that dictates what they do and that can be influenced by others.

## How to run it

### Locally using the shell

```
sen run config/4_school/1_school_two_classrooms_one_component.yaml
```

This creates two classrooms that live in the same Sen component.

Each classroom is published in its own bus ("school.primary" and "school.secondary").

You can use the shell to monitor the status or call methods.

### Locally using the explorer

This is the same as the previous example, but also loading the explorer to have an easier time
following the updates.

```
sen run config/4_school/2_school_two_classrooms_one_component_exp.yaml
```

With the explorer the state should be way easier to follow. You can also use plots and explore the
value that different properties take as time advances.

Additionally, you can use the event explorer to have a look at the different events that get
produced during the execution (remember to check the events you are interested in).

### Locally using the Sen Query Language (SQL)

This example loads the first configuration, but configures the shell with a dynamic query.

```
sen run config/4_school/3_school_two_classrooms_one_component_query.yaml
```

You should be able to see how the list of present objects changes as they match the query.

### Over the network using the eth and a query

This is the same as the previous example, but you will need to start two processes.

First run:

```
sen run config/4_school/4_school_shell_eth_query.yaml
```

Then, in another terminal or command prompt, run:

```
sen run config/4_school/4_school_two_classrooms_one_component_query.yaml
```

### Locally, but distributed over two Sen components

This is like the first example, but using two threads. One per classroom.

First run:

```
sen run config/4_school/5_school_two_classrooms_two_components_query.yaml
```

### Locally, but distributed over two Sen components, using the explorer.

This is like the previous example, now with the explorer to have a better look..

```
sen run config/4_school/6_school_two_classrooms_two_components_query_exp.yaml
```

### Over the network with a remote teacher

Here we create two instances of the same classroom, one without teacher. They will discover the
teacher over the network.

First run:

```
sen run config/4_school/7_school_one_classroom_no_teacher_eth.yaml
```

Then, in another terminal or command prompt, run:

```
sen run config/4_school/7_school_one_classroom_teacher_eth.yaml
```
