# Workflow

The typical workflow when using Sen in large projects is as follows:

1. Subsystems define the set of types and classes that will be used to interface (their public API).
   Here they annotate those definitions with behavioral contracts, constraints and
   quality-of-service parameters for communication.
2. These interface definitions are used by the software developers (and there could be multiple
   teams in parallel) to implement the functional logic. The code generator creates the glue code
   that takes care of repetitive or error-prone logic, so that developers can focus on the important
   things. It also generates the required documentation.
3. The functional implementation is then compiled into packages, tested (typically in a CI
   environment) and stored as common assets.
4. Integrators (with the help of the subsystem teams) define where and how to instantiate the
   required functionality, by producing the relevant configuration files. Packages and configuration
   files can be combined into deployable units to be ready for execution.
5. Integrators then deploy the processes and validate the system behavior, providing feedback to the
   rest of the team. Note that this integration testing work should be automated as much as
   possible, and moved to the project-specific CI system.

![Screenshot](../assets/images/workflow_light.svg#only-light){: style="width:1200px;"}
![Screenshot](../assets/images/workflow_dark.svg#only-dark){: style="width:1200px;"}

We have depicted some roles here as different people, but in reality you will often see the same
person wearing all those hats (depending on the size and complexity of the project).

The whole of this should run in minutes on a developer's own machine, so the loop stays short. That
is why the build, deployment and test tooling are integrated as tightly as they are.
