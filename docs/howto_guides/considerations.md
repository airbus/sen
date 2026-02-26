# Design Considerations

With Sen, you will be able to expand your options when designing your system. This page compiles
some notes, considerations and tips you can use when doing so.

## Bridging Domains

![Screenshot](../assets/images/gateway_light.svg#only-light){: style="width:600px; float: right"}
![Screenshot](../assets/images/gateway_dark.svg#only-dark){: style="width:600px; float: right"}

It is possible to bridge Sen to other IPC systems and communication solutions, just as Sen can be
bridged to object systems. By doing so, existing Sen applications do not need to know if they are
connected with some other system. All they see are objects with methods and events. Note that these
applications do not even need to be re-compiled.

## About Microservices

Microservices are services that may be deployed separately, modeled around a business domain. A
service wraps functionality and makes it available to other services via networks; you may create a
more complex system from these building blocks. Microservices are an architecture choice that
focuses on providing you with a variety of solutions to the difficulties you may have.

From the outside, a single microservice seems like a black box. It hosts business functionality on
one or more network endpoints using the protocols that are most suited. Consumers, whether
microservices or different sorts of programs, can get to this capability through these networked
endpoints. Internal implementation specifics (such as the technology used to write the service or
how data is kept) are completely hidden from the outer world.

Microservices embody the principle of encapsulation. This means hiding as much information as
possible inside a component while only revealing a minimal amount through external interfaces. This
facilitates a distinct separation between elements that can change easily and those that are more
difficult to change.

The hidden implementation of a microservice can be modified without restrictions as long as the
networked interfaces it exposes remain unchanged in a way that is compatible with previous versions.
Modifications made within the confines of a microservice should not have any impact on a customer
located farther upstream, thus allowing for the independent release of functionality. This is
crucial for enabling our microservices to be developed independently and deployed as needed.

Having clear, stable service boundaries that don’t change when the internal implementation changes
results in systems that have looser coupling and stronger cohesion.

> As useful as services are to the scalability and develop-ability of a system, they are not, in and
> of themselves, the most architecturally significant element. The architecture of a system is
> defined by the boundaries drawn within that system, and by the dependencies that cross those
> boundaries.

### Key Concepts of Microservices

**Independent Deployability**

Is the idea that we can make a modification to a microservice, deploy it, and make that change
available to our users without having to deploy any other microservices. More importantly, it is not
only that we can do this; it is also how you handle deployments in your system. It's a discipline
you use as your default release strategy.

**Modeled around a business domain**

Techniques such as domain-driven design can help you structure your code so that it better
represents the real-world domain in which the software runs. With microservice architectures, we
employ the same concept to design service boundaries. By designing services around business domains,
we can make it easier to roll out new functionality and recombine microservices in novel ways to
provide new functionality to our users.

**Ownership of their state**

In a microservice, hiding the internal state is similar to the approach of encapsulation in
object-oriented programming. Encapsulation of data in object-oriented systems depicts information
hiding in action. We want to think of our services as end-to-end slices of business functionality
that, if applicable, include a user interface (UI), business logic, and data. This is because we
want to minimize the work required to update business-related functions. The encapsulation of data
and behavior in this manner results in strong coherence of business functionality.

**Size**

"How big should a microservice be?" is a frequently asked question. Don't worry too much about size.
When you initially start off, it's critical that you concentrate on two main areas.

- First, how many microservices can you manage? As you add additional services, your system will get
  more complex, and you will need to learn new skills (and even embrace new technology) to keep up.
  Moving to microservices will add new sources of complexity, as well as all of the associated
  issues. For this reason, we should aim for a progressive transition to a microservice design.

- Second, how do you establish microservice boundaries so that you get the most out of them while
  avoiding a hopelessly linked mess? These are far more significant items to consider when embarking
  on your quest.

### Advantages of Microservices

![Screenshot](../assets/images/advantage_seal_light.svg#only-light){: style="width:200px;
float:right"} ![Screenshot](../assets/images/advantage_seal_dark.svg#only-dark){:
style="width:200px; float: right"}

Microservices, by combining the concepts of information hiding and domain-driven design with the
power of distributed systems, can help give considerable advantages over other types of distributed
architecture.

**Technology Heterogeneity.**

With a system made up of several, interacting microservices, we can choose to use different
technologies within each one. This enables us to select the best tool for each project rather than a
more standardized, one-size-fits-all strategy that frequently results in the lowest common
denominator.

**Robustness.**

The bulkhead is an important notion for increasing the robustness of your application. A system
component may fail, but as long as the failure does not cascade, you can isolate the issue and
continue to operate the system. If a monolithic system fails, the entire system ceases to function.
A monolithic system can run on numerous computers to lessen the risk of failure, whereas
microservices allow us to construct systems that can handle the complete failure of some constituent
services and degrade functionality accordingly.

**Scaling.**

With a massive, monolithic service, we must scale everything simultaneously. Perhaps one little
component of our larger system is performance constrained, but if that behavior is locked up in a
massive monolithic program, we must address scaling everything as a whole. With smaller services, we
can scale only the services that require it, allowing us to run the rest of the system on smaller,
less powerful hardware.

**Ease of Deployment.**

To release a single line change in a million-line monolithic program, the entire application must be
released. That may be a large-scale, high-risk deployment. In practice, such deployments occur
infrequently due to understandable fear. With microservices, we may modify a single service and
deploy it independently of the rest of the system. This allows us to get our code deployed faster.

If an issue arises, it may be rapidly isolated to a specific service, allowing for easy rollback. It
also implies that we may release our new capabilities to clients more quickly. This is one of the
primary reasons why companies like Amazon and Netflix employ these architectures—to remove as many
barriers as possible to getting software out the door.

**Organizational Alignment.**

Many of us have encountered challenges with huge teams and code bases. These issues can be worsened
when the team is dispersed. We also know that smaller teams working with smaller code bases are more
productive.

Microservices enable us to better align our architecture with our organization, reducing the amount
of people working on any one codebase and achieving the ideal balance of team size and productivity.
Microservices also enable us to alter ownership of services as the organization changes, allowing us
to keep the architecture and organization aligned in the future.

**Composability.**

One of the primary promises of distributed systems and service-oriented architectures is that we
will enable the reuse of functionality. We can leverage microservices to consume our functionality
in a variety of ways for multiple purposes. This is especially significant when we consider how our
customers use our product.

With microservices, think of us opening up seams in our system that are addressable by outside
parties. As circumstances change, we can build applications in different ways. In a monolithic
program, we frequently have one coarse-grained seam that can be accessed from the outside. If we
want to break it up into something more helpful, we will need a big hammer.

### Microservices and Containers

Ideally, you should execute each microservice instance independently. This ensures that issues in
one microservice do not affect another, such as when it consumes all the CPU. Virtualization is one
method for creating separate execution environments on existing hardware, but traditional
virtualization solutions can be fairly heavy when considering the scale of our microservices.

![Screenshot](../assets/images/container_light.svg#only-light){: style="width:350px; float: right"}
![Screenshot](../assets/images/container_dark.svg#only-dark){: style="width:350px; float: right"}

Containers, on the other hand, offer a far more lightweight method of provisioning isolated
execution for service instances, resulting in faster spin-up times for new container instances while
also being significantly more cost-effective for many architectures.

After you start playing around with containers, you'll find you need something to manage these
containers across multiple underlying machines. Container orchestration technologies, such as
Kubernetes, provide just that, allowing you to deploy container instances in such a way that your
service has the stability and throughput it requires while also making optimal use of underlying
equipment.

## About Monoliths

**The Single-Process Monolith**

When discussing monoliths, the most popular example is a system that deploys all of its code as a
single process. You may run many instances of this process for robustness or scaling purposes, but
all the code is contained within a single process. In truth, these single-process systems can
function as simple distributed systems in their own right because they almost always end up reading
data from or saving data in a database, or presenting information to the web or GUIs.

**The Modular Monolith.**

The modular monolith is a subset of the single-process monolith in which the single process is
divided into different modules. Each module can be worked on alone, but all must be combined for
deployment.

The concept of splitting down software into modules is not new; modular software has its roots in
work on structured programming in the 1970s, and even further back. For many businesses, the modular
monolith might be an ideal option. If the module borders are well-defined, it is possible to perform
a high degree of parallel work while avoiding the issues of a more distributed microservice
architecture through a much simpler deployment topology.

![Screenshot](../assets/images/monolith_light.svg#only-light){: style="width:500px; float: right"}
![Screenshot](../assets/images/monolith_dark.svg#only-dark){: style="width:500px; float: right"}

**The Distributed Monolith**

A distributed monolith is a system made up of several services that, for whatever reason, must all
be deployed simultaneously. A distributed monolith may fit the definition of a SOA, but it
frequently fails to live up to its promises.

They have all the disadvantages of a distributed system, and the disadvantages of a single-process
monolith, without having enough of the upsides of either.

Distributed monoliths frequently occur in environments where ideas such as information hiding and
business functionality cohesiveness have not received adequate attention. Instead, strongly linked
designs cause changes to propagate across service boundaries, and relatively minor modifications
that appear to be local in scope disrupt other portions of the system.

## Architectural Guidelines

The primary purpose of architecture is to support the life cycle of the system by minimizing the
cost and maximizing programmer productivity. Good architecture renders the system comprehensible,
developable, maintainable, and deployable.

**Development**

A software system that is hard to develop is not likely to have a long and healthy lifetime. The
architecture of the system should make it easy for the team to develop it. Different team structures
imply different architectural decisions.

On the one hand, a small team of five developers can quite effectively work together to develop a
monolithic system without well-defined components or interfaces. Such team would likely find the
strictures of an architecture something of an impediment during the early days of development. This
is likely why so many systems lack good architecture: They began with none, because the team was
small and did not want the impediment of a superstructure.

On the other hand, a system being developed by five different teams cannot make progress unless the
system is divided into well-defined components with reliably stable interfaces. The architecture of
that system will likely evolve into five components if no other factors are considered.

**Deployment**

To be effective, a software system must be deployable. The higher the cost of deployment, the less
useful the system is. A goal of a software architecture should be to make a system conveniently
deployable with a single action.

Unfortunately, deployment strategy is seldom considered during initial development. This leads to
architectures that may make the system easy to develop, but leave it very difficult to deploy.

**Operation**

The impact of architecture on system operation tends to be less dramatic than the impact of
architecture on development, deployment, and maintenance.

Architecture should reveal operation. It should elevate the use cases, the features, and the
required behaviors of the system to first-class entities that are visible landmarks for the
developers. This simplifies the understanding of the system and, therefore, greatly aids in
development and maintenance.

**Maintenance**

Of all the aspects of a software system, maintenance is the most costly. The never-ending parade of
new features and the inevitable trail of defects and corrections consume vast amounts of human
resources. A conscientiously thought-through architecture greatly reduces costs.

By separating the system into components, and isolating those components through stable interfaces,
it is possible to illuminate the pathways for future features and vastly reduce the risk of
inadvertent breakage.

Software design requires combining multiple perspectives and evaluating their applicability in
different contexts. In this section, we define some general ideas to guide the design decisions.

**Aim for simplicity and clarity of intent**

Simplicity and clarity are applicable to any software development, but are especially relevant in
environments with real-time constraints. In this context, we weigh every design decision that adds
cognitive or implementation-related complexity in terms of its value. Enforcement of this guideline
is difficult but possible through attentive review and firm adherence to coding standards. Failing
to this guideline results in technical debt that ends up in refactoring.

**No unnecessary abstraction layers**

The efficient usage of memory and compute resources is essential, and the “abstraction penalty”
should be avoided. Abstractions shall only exist when the motivation is distinctly justified. Even
then, techniques that reduce or eliminate abstraction layers such as generic programming (where the
compiler takes care of it) or explicit code generation (where the code generator deals with removing
indirections) mitigate the penalties.

**Solve problems only once and make solutions easily available**

Different solutions to the same problem are costly and prone to error. Their usage, scope, and
context usually define how thoroughly they get tested and validated.

**Automate as much as possible**

Repetitive, manually written code is tedious and prone to error. Therefore, creating tools to write
it with instruction models is undoubtedly preferred. Automation is not limited to C/C++ code, but
naturally includes build and configuration files.

**Prefer declarative definitions**

Declarative definitions express facts and conscious intent. Intent can be exploited by tools to
generate the most appropriate code, and by humans to better recognize the purpose. These definitions
apply to code, models, and configuration files.

**Enable composition**

Reusable building blocks with a clearly defined interface and sacrificing performance or safety.
More importantly, they enable scaling our system, codebase, and development efforts. In this
environment, composition shall be possible at design and compile-time moving parts.

**Aim for testability at every level**

This results in:

- Elements that are self-standing for instantiation, stimulation, and individual testing.
- Dependency injection without polluting the interface.
- Reliance on scripting tools for high-level integration and orchestration tests.
- Instrumented code that creates continuous performance and endurance tests, and regression
  detections.

Testing shall not be a second thought, but a primary need for both Sen and functional components.
Testing must apply existing, well-known patterns for exercising the code. To ensure that your code
is easy to test:

- Isolate the function from the context, make the context mockable, apply dependency injection, and
  systematically inject errors.
- Rely on unit tests to detect problems as quickly as possible.
- Declare pre- and post-conditions and automate them for validation at every possible level.
- Rely on integration tests to verify contracts between components.
- Run the tests in the target HW environment as the critical last step. If it does not work in the
  target, it does not work, but requiring the target HW for testing basic logic is not sufficient.

**Tooling follows method, not the other way around**

If a tool is not in line with the methodology of choice, it shall be adapted or replaced. This
guideline is crucial in any development and extends to the build system, toolchains, CI, and so
forth.

**Prioritize validation by design-time > compile-time > run-time**

Prevent bugs as early as possible. Design-time checkers are the first and most powerful weapon
against bugs. Design-time checkers prevent bugs before any code is written, and the tooling has
access to system-level and domain-specific considerations that the compiler lacks. That said, the
compiler is a powerful ally, and it has a better understanding of the final code than the tool that
generated the code. Therefore, both the generated and manually written code should use compile-time
assertions to prevent wrong assumptions (which is especially relevant when switching compilers,
architecture, or operating systems).

**Surface design decisions**

Design is a fundamental part of any serious product development from a qualification perspective,
and it makes understanding the system easier. To have a software design, we first need a language in
which we will express the concepts that our architecture is built with. This means having an
architectural framework that is backed by a SW infrastructure that ensures that we are not “building
a castle in the air.” Our design approach must be solid, up to date with the code, transparent to
the tooling, traceable, easy to change, easy to validate, and resilient to human error.

Sen (starting with this document) shall enforce design decisions to be visible, documented, and
structured.
