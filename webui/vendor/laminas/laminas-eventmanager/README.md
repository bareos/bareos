# laminas-eventmanager

[![Build Status](https://travis-ci.org/laminas/laminas-eventmanager.svg?branch=master)](https://travis-ci.org/laminas/laminas-eventmanager)
[![Coverage Status](https://coveralls.io/repos/laminas/laminas-eventmanager/badge.svg?branch=master)](https://coveralls.io/r/laminas/laminas-eventmanager?branch=master)

The `Laminas\EventManager` is a component designed for the following use cases:

- Implementing simple subject/observer patterns.
- Implementing Aspect-Oriented designs.
- Implementing event-driven architectures.

The basic architecture allows you to attach and detach listeners to named events,
both on a per-instance basis as well as via shared collections; trigger events;
and interrupt execution of listeners.


- File issues at https://github.com/laminas/laminas-eventmanager/issues
- Documentation is at https://docs.laminas.dev/laminas-eventmanager

## Benchmarks

We provide scripts for benchmarking laminas-eventmanager using the
[Athletic](https://github.com/polyfractal/athletic) framework; these can be
found in the `benchmarks/` directory.

To execute the benchmarks you can run the following command:

```bash
$ vendor/bin/athletic -p benchmarks
```
