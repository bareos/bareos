<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Controller;

use Laminas\EventManager\EventInterface as Event;
use Laminas\EventManager\EventManager;
use Laminas\EventManager\EventManagerAwareInterface;
use Laminas\EventManager\EventManagerInterface;
use Laminas\Http\PhpEnvironment\Response as HttpResponse;
use Laminas\Http\Request as HttpRequest;
use Laminas\Mvc\InjectApplicationEventInterface;
use Laminas\Mvc\MvcEvent;
use Laminas\ServiceManager\ServiceLocatorAwareInterface;
use Laminas\ServiceManager\ServiceLocatorInterface;
use Laminas\Stdlib\DispatchableInterface as Dispatchable;
use Laminas\Stdlib\RequestInterface as Request;
use Laminas\Stdlib\ResponseInterface as Response;

/**
 * Abstract controller
 *
 * Convenience methods for pre-built plugins (@see __call):
 *
 * @method \Laminas\View\Model\ModelInterface acceptableViewModelSelector(array $matchAgainst = null, bool $returnDefault = true, \Laminas\Http\Header\Accept\FieldValuePart\AbstractFieldValuePart $resultReference = null)
 * @method bool|array|\Laminas\Http\Response fileprg(\Laminas\Form\FormInterface $form, $redirect = null, $redirectToUrl = false)
 * @method bool|array|\Laminas\Http\Response filePostRedirectGet(\Laminas\Form\FormInterface $form, $redirect = null, $redirectToUrl = false)
 * @method \Laminas\Mvc\Controller\Plugin\FlashMessenger flashMessenger()
 * @method \Laminas\Mvc\Controller\Plugin\Forward forward()
 * @method mixed|null identity()
 * @method \Laminas\Mvc\Controller\Plugin\Layout|\Laminas\View\Model\ModelInterface layout(string $template = null)
 * @method \Laminas\Mvc\Controller\Plugin\Params|mixed params(string $param = null, mixed $default = null)
 * @method \Laminas\Http\Response|array prg(string $redirect = null, bool $redirectToUrl = false)
 * @method \Laminas\Http\Response|array postRedirectGet(string $redirect = null, bool $redirectToUrl = false)
 * @method \Laminas\Mvc\Controller\Plugin\Redirect redirect()
 * @method \Laminas\Mvc\Controller\Plugin\Url url()
 * @method \Laminas\View\Model\ConsoleModel createConsoleNotFoundModel()
 * @method \Laminas\View\Model\ViewModel createHttpNotFoundModel(Response $response)
 */
abstract class AbstractController implements
    Dispatchable,
    EventManagerAwareInterface,
    InjectApplicationEventInterface,
    ServiceLocatorAwareInterface
{
    /**
     * @var PluginManager
     */
    protected $plugins;

    /**
     * @var Request
     */
    protected $request;

    /**
     * @var Response
     */
    protected $response;

    /**
     * @var Event
     */
    protected $event;

    /**
     * @var EventManagerInterface
     */
    protected $events;

    /**
     * @var ServiceLocatorInterface
     */
    protected $serviceLocator;

    /**
     * @var null|string|string[]
     */
    protected $eventIdentifier;

    /**
     * Execute the request
     *
     * @param  MvcEvent $e
     * @return mixed
     */
    abstract public function onDispatch(MvcEvent $e);

    /**
     * Dispatch a request
     *
     * @events dispatch.pre, dispatch.post
     * @param  Request $request
     * @param  null|Response $response
     * @return Response|mixed
     */
    public function dispatch(Request $request, Response $response = null)
    {
        $this->request = $request;
        if (!$response) {
            $response = new HttpResponse();
        }
        $this->response = $response;

        $e = $this->getEvent();
        $e->setRequest($request)
          ->setResponse($response)
          ->setTarget($this);

        $result = $this->getEventManager()->trigger(MvcEvent::EVENT_DISPATCH, $e, function ($test) {
            return ($test instanceof Response);
        });

        if ($result->stopped()) {
            return $result->last();
        }

        return $e->getResult();
    }

    /**
     * Get request object
     *
     * @return Request
     */
    public function getRequest()
    {
        if (!$this->request) {
            $this->request = new HttpRequest();
        }

        return $this->request;
    }

    /**
     * Get response object
     *
     * @return Response
     */
    public function getResponse()
    {
        if (!$this->response) {
            $this->response = new HttpResponse();
        }

        return $this->response;
    }

    /**
     * Set the event manager instance used by this context
     *
     * @param  EventManagerInterface $events
     * @return AbstractController
     */
    public function setEventManager(EventManagerInterface $events)
    {
        $className = get_class($this);

        $nsPos = strpos($className, '\\') ?: 0;
        $events->setIdentifiers(array_merge(
            [
                __CLASS__,
                $className,
                substr($className, 0, $nsPos)
            ],
            array_values(class_implements($className)),
            (array) $this->eventIdentifier
        ));

        $this->events = $events;
        $this->attachDefaultListeners();

        return $this;
    }

    /**
     * Retrieve the event manager
     *
     * Lazy-loads an EventManager instance if none registered.
     *
     * @return EventManagerInterface
     */
    public function getEventManager()
    {
        if (!$this->events) {
            $this->setEventManager(new EventManager());
        }

        return $this->events;
    }

    /**
     * Set an event to use during dispatch
     *
     * By default, will re-cast to MvcEvent if another event type is provided.
     *
     * @param  Event $e
     * @return void
     */
    public function setEvent(Event $e)
    {
        if (!$e instanceof MvcEvent) {
            $eventParams = $e->getParams();
            $e = new MvcEvent();
            $e->setParams($eventParams);
            unset($eventParams);
        }
        $this->event = $e;
    }

    /**
     * Get the attached event
     *
     * Will create a new MvcEvent if none provided.
     *
     * @return MvcEvent
     */
    public function getEvent()
    {
        if (!$this->event) {
            $this->setEvent(new MvcEvent());
        }

        return $this->event;
    }

    /**
     * Set serviceManager instance
     *
     * @param  ServiceLocatorInterface $serviceLocator
     * @return void
     */
    public function setServiceLocator(ServiceLocatorInterface $serviceLocator)
    {
        $this->serviceLocator = $serviceLocator;
    }

    /**
     * Retrieve serviceManager instance
     *
     * @return ServiceLocatorInterface
     */
    public function getServiceLocator()
    {
        return $this->serviceLocator;
    }

    /**
     * Get plugin manager
     *
     * @return PluginManager
     */
    public function getPluginManager()
    {
        if (!$this->plugins) {
            $this->setPluginManager(new PluginManager());
        }

        $this->plugins->setController($this);
        return $this->plugins;
    }

    /**
     * Set plugin manager
     *
     * @param  PluginManager $plugins
     * @return AbstractController
     */
    public function setPluginManager(PluginManager $plugins)
    {
        $this->plugins = $plugins;
        $this->plugins->setController($this);

        return $this;
    }

    /**
     * Get plugin instance
     *
     * @param  string     $name    Name of plugin to return
     * @param  null|array $options Options to pass to plugin constructor (if not already instantiated)
     * @return mixed
     */
    public function plugin($name, array $options = null)
    {
        return $this->getPluginManager()->get($name, $options);
    }

    /**
     * Method overloading: return/call plugins
     *
     * If the plugin is a functor, call it, passing the parameters provided.
     * Otherwise, return the plugin instance.
     *
     * @param  string $method
     * @param  array  $params
     * @return mixed
     */
    public function __call($method, $params)
    {
        $plugin = $this->plugin($method);
        if (is_callable($plugin)) {
            return call_user_func_array($plugin, $params);
        }

        return $plugin;
    }

    /**
     * Register the default events for this controller
     *
     * @return void
     */
    protected function attachDefaultListeners()
    {
        $events = $this->getEventManager();
        $events->attach(MvcEvent::EVENT_DISPATCH, [$this, 'onDispatch']);
    }

    /**
     * Transform an "action" token into a method name
     *
     * @param  string $action
     * @return string
     */
    public static function getMethodFromAction($action)
    {
        $method  = str_replace(['.', '-', '_'], ' ', $action);
        $method  = ucwords($method);
        $method  = str_replace(' ', '', $method);
        $method  = lcfirst($method);
        $method .= 'Action';

        return $method;
    }
}
