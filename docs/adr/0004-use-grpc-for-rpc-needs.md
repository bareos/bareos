# Try to use grpc for inter process communication & remote procedure calls

* Status: accepted
* Deciders: Sebastian Sura, ...
* Date: 2025-01-10

## Context and Problem Statement

Whenever the need arises for one of our binaries to talk to another one, we have to think about how they are going to communicate.
We want to make this decision easier to ourselves by having a default rpc implementation that we reach for.

## Decision Drivers

* avoid unnecessary work
* make it easier to integrate into other environments
* less adhoc rpc implementations is less maintenance work for us

## Considered Options

* grpc
* json rpc
* bsock

## Decision Outcome

Chosen option: grpc, because
* it is well supported in C++ and python,
* is actively maintained, and
* was relatively painless to integrate.

### Positive Consequences

* It is clearer on how to do ipc/rpc
* We may be able to design our architecture around some advanced features of grpc
* Less thought needs to be put into how to develop a good ipc/rpc

### Negative Consequences

* Grpc is overkill for a lot of our applications
* We have to ensure that all build targets have access to the protobuf/grpc libraries

## Pros and Cons of the Options

### grpc

Good:
* Well maintained
* Comes with integrations into lots of languages
* is used very often

Bad:
* grpc has lots of features that we are not interested in
* grpc main focus is _remote_ procedure calls, not local ipc
* grpc is basically a framework that handles everything: accepting connections, sending data, serializing messages, etc.
* Lots of boiler plate code for generating the necessary code
* The team has very little experience with grpc

### json rpc

Good:
* There are lots of json encoding libraries that we can choose to use
* Allows us to choose our own connection method, e.g. using websockets, normal sockets, pipes, etc.

Bad:
* There are lots of json libraries, which may introduce subtle bugs
* We would still have to take care of the message delivery on our own
* Serializing & deserializing to json is slow
* This approach is not great for big binary blobs
* There is no well maintained rpc library that would handle type checking/message checking

### bsock

Experience has shown that the bsock protocol has some big limitations, especially when it comes to extensibility.
