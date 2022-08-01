# The MvcEvent

The MVC layer of Zend Framework 2 incorporates and utilizes a custom `Zend\EventManager\Event`
implementation -`Zend\Mvc\MvcEvent`. This event is created during
`Zend\Mvc\Application::bootstrap()` and is passed directly to all the events that method triggers.
Additionally, if your controllers implement the `Zend\Mvc\InjectApplicationEventInterface`,
`MvcEvent` will be injected into those controllers.

The `MvcEvent` adds accessors and mutators for the following:

- `Application` object.
- `Request` object.
- `Response` object.
- `Router` object.
- `RouteMatch` object.
- Result - usually the result of dispatching a controller.
- `ViewModel` object, typically representing the layout view model.

The methods it defines are:

- `setApplication($application)`
- `getApplication()`
- `setRequest($request)`
- `getRequest()`
- `setResponse($response)`
- `getResponse()`
- `setRouter($router)`
- `getRouter()`
- `setRouteMatch($routeMatch)`
- `getRouteMatch()`
- `setResult($result)`
- `getResult()`
- `setViewModel($viewModel)`
- `getViewModel()`
- `isError()`
- `setError()`
- `getError()`
- `getController()`
- `setController($name)`
- `getControllerClass()`
- `setControllerClass($class)`

The `Application`, `Request`, `Response`, `Router`, and `ViewModel` are all injected during the
`bootstrap` event. Following the `route` event, it will be injected also with the `RouteMatch`
object encapsulating the results of routing.

Since this object is passed around throughout the MVC, it is a common location for retrieving the
results of routing, the router, and the request and response objects. Additionally, we encourage
setting the results of execution in the event, to allow event listeners to introspect them and
utilize them within their execution. As an example, the results could be passed into a view
renderer.

## Order of events

The following events are triggered, in the following order:

Those events are extensively describe in the following sections.

## MvcEvent::EVENT\_BOOTSTRAP

### Listeners

The following classes are listening to this event (they are sorted from higher priority to lower
priority):

### Triggerers

This event is triggered by the following classes:

## MvcEvent::EVENT\_ROUTE

### Listeners

The following classes are listening to this event (they are sorted from higher priority to lower
priority):

### Triggerers

This event is triggered by the following classes:

## MvcEvent::EVENT\_DISPATCH

### Listeners

The following classes are listening to this event (they are sorted from higher priority to lower
priority):

#### Console context only

Those listeners are only attached in a Console context:

#### Http context only

Those listeners are only attached in a Http context:

#### All contexts

Those listeners are attached for both contexts:

### Triggerers

This event is triggered by the following classes:

## MvcEvent::EVENT\_DISPATCH\_ERROR

### Listeners

The following classes are listening to this event (they are sorted from higher priority to lower
priority):

#### Console context only

#### Http context only

Those listeners are only attached in a Http context:

#### All contexts

Those listeners are attached for both contexts:

### Triggerers

## MvcEvent::EVENT\_RENDER

### Listeners

The following classes are listening to this event (they are sorted from higher priority to lower
priority):

#### Console context only

Those listeners are only attached in a Console context:

#### Http context only

Those listeners are only attached in a Http context:

### Triggerers

This event is triggered by the following classes:

## MvcEvent::EVENT\_RENDER\_ERROR

### Listeners

The following classes are listening to this event (they are sorted from higher priority to lower
priority):

#### Console context only

Those listeners are only attached in a Console context:

#### Http context only

Those listeners are only attached in a Http context:

### Triggerers

This event is triggered by the following classes:

## MvcEvent::EVENT\_FINISH

### Listeners

The following classes are listening to this event (they are sorted from higher priority to lower
priority):

### Triggerers

This event is triggered by the following classes:
