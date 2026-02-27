# The Sen Query Language

## The Challenge

We have two related needs:

1. The infrastructure should prevent saturating the network with redundant or unnecessary traffic.
2. Users need an easy way to find the objects they are interested in. And this interest may change
   over time.

Sen automatically optimizes the communication by only sending the changes with respect to the
previous state. For example, imagine you have an object with 10 properties, and you have only
changed 3 of them in the current execution cycle. Sen will only send those 3 values (and only if
they differ from the previous). This is important because it greatly reduces network traffic.

In any case, the idea is to go beyond that, and only send those updates towards the processes which
are interested in the information. Most traditional systems simply send the information and let the
consumers decide if to use or ignore it. This is mostly prevalent in systems that rely on multicast,
as the producer doesn't see any difference between having one or many consumers. But things change
when we have multiple quality-of-service configurations (UDP unicast, multicast and TCP), because
then the producer needs to do more work and dedicate time and memory to each consumer. How do we do
to make the producer aware of which participant is interested in which object and under which
conditions?

## The Solution

We do as follows:

1. When you publish an object, Sen takes note, but doesn't notify anyone straight away.
2. When you connect to a bus, Sen takes note, but you don't receive any objects straight away.
3. You tell Sen in which kind of objects you are interested, and under which circumstances. Sen
   takes note, and notifies the connected participants about it. It will also notify any new
   participant joining later.
4. Sen participants will automatically take note of your interests, and check if any of the objects
   that they publish is of your interest. If they do, the participant will automatically let you
   know, and you will be notified about the existence of the object.

This approach is usually called "producer level filtering", as the producer is the one who evaluates
when to send the information to the different consumers. Normally, this is the most efficient way of
working, as we avoid traffic from happening in the first place.

You might be wondering about the performance impact on the producers that have to do those checks,
and the trade-offs of this approach. Consider that:

- When sending updates (property changes and events), producers consume resources. We need to keep
  track of the deltas, serialize the messages, and transmit the data (individually to each
  participant in case of UDP unicast or TCP).
- The implementation of the checks in the producers is highly optimized.
- The checks are only done when needed. For example, if your interest depends on an object's name,
  class name, or some static property, the check will only be done once (when the object is
  published).
- Even if the check depends on a property that might change, Sen will only compute it when the
  property actually changes.
- This is an "optional" feature. You can always express interest in everything and do the filtering
  in the consumer. When the interest is in "everything", producers do not perform any check and
  broadcast the object existence.

Note that to achieve this, Sen defines a way for you to *declare* the conditions that any object
needs to fulfil. This *externalization* of information comes with some advantages:

1. The conditions aren't "buried" in the code, but made explicit. This makes code easier to
   understand and maintain.
2. You can change the conditions by simply changing a string, which can even be passed as a
   configuration parameter to your application/component.
3. Static analysis tools are able to provide insight on the objects that you are going to receive,
   and create diagrams of the system layout, compute worst-case networking situations, detect
   configuration errors, etc.
4. Consumers get the notifications as soon as the data is available, as the producers evaluate the
   conditions at their own rate, and as soon as the changes are made.

The **Sen Query Language** is what you use to express interest in objects. Note that the name of the
thing is SQL when abbreviated, and that's a happy coincidence because it's very similar.

```sql title="Example using the name of an object"
SELECT rpr.PhysicalEntity FROM se.env WHERE name = "ownship"
```

You can see that the classes are analogous to the selected elements (we are selecting objects),
buses are analogous to tables, and properties are analogous to columns.

You can also use other expressions, and combination of expressions:

```sql title="Example using other expressions"
SELECT school.Student
  FROM school.primary
 WHERE focusLevel BETWEEN 0.0 AND 0.5

SELECT rpr.PhysicalEntity
  FROM se.env
 WHERE spatial.SpatialFPStruct.lat >= 34.0
   AND spatial.SpatialFPStruct.lon >= 4.0

SELECT rpr.Aircraft
  FROM se.env
 WHERE forceIdentifier IN ("friendly", "neutral", "opposing")
   AND (isActive OR spatial.SpatialFPStruct.lon < 1.0)

SELECT se.Aircraft
FROM   se.env
WHERE  currentSpeed BETWEEN commandedSpeed - 10.0 AND
                            commandedSpeed + 10.0
```

The `SELECT` expression syntax is as follows:

```
SELECT class_name
FROM bus_name
WHERE condition
```

The operators in the `WHERE` clause are:

| Operator          | Description                         |
| ----------------- | ----------------------------------- |
| `=`               | Equal                               |
| `>`               | Greater than                        |
| `<`               | Lower than                          |
| `>=`              | Greater than or equal               |
| `<=`              | Lower than or equal                 |
| `BETWEEN x AND y` | Between a certain range             |
| `IN (x, y, ...)`  | To specify multiple possible values |

The `WHERE` clause can be combined with `AND`, `OR`, and `NOT` operators.

The `AND` and `OR` operators are used to filter objects based on more than one condition:

- The `AND` operator selects an object if all the conditions separated by `AND` are `true`.
- The `OR` operator selects an object if any of the conditions separated by `OR` is `true`.

The `NOT` operator selects an object if the condition(s) is `NOT true`.

Regarding values:

- Strings be "double-quoted".
- Booleans can be `true` or `false`.
- Integers as usual.
- Real numbers use the dot for the decimal part (as usual).
- Enumerations are quoted and not qualified (i.e. use "friendly" instead of
  "rpr.ForceIdentifierEnum.friendly").

Regarding variables:

- You can use a property name.
- You can use the "name" pseudo-property, which contains a string.
- You can use the "id" pseudo-property, which contains an unsigned integer.
- If the property or field is a structure, you need to refer to a field (i.e. position.x).
- If the property or field is a variant, you need to specify the type (i.e.
  spatial.SpatialFPStruct.lon).
- If the variant doesn't contain a value of the specified type, the operator returns `false`.
