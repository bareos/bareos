# zend-mvc

[![Build Status](https://secure.travis-ci.org/zendframework/zend-mvc.svg?branch=master)](https://secure.travis-ci.org/zendframework/zend-mvc)
[![Coverage Status](https://coveralls.io/repos/zendframework/zend-mvc/badge.svg?branch=master)](https://coveralls.io/r/zendframework/zend-mvc?branch=master)

`Zend\Mvc` is a brand new MVC implementation designed from the ground up for
Zend Framework 2, focusing on performance and flexibility.

The MVC layer is built on top of the following components:

- `Zend\ServiceManager` - Zend Framework provides a set of default service
  definitions set up at `Zend\Mvc\Service`. The ServiceManager creates and
  configures your application instance and workflow.
- `Zend\EventManager` - The MVC is event driven. This component is used
  everywhere from initial bootstrapping of the application, through returning
  response and request calls, to setting and retrieving routes and matched
  routes, as well as render views.
- `Zend\Http` - specifically the request and response objects, used within:
  `Zend\Stdlib\DispatchableInterface`. All “controllers” are simply dispatchable
  objects.


- File issues at https://github.com/zendframework/zend-mvc/issues
- Documentation is at https://zendframework.github.io/zend-mvc/
