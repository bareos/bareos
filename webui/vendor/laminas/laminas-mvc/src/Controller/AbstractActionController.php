<?php

/**
 * @see       https://github.com/laminas/laminas-mvc for the canonical source repository
 * @copyright https://github.com/laminas/laminas-mvc/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-mvc/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Mvc\Controller;

use Laminas\Http\Response as HttpResponse;
use Laminas\Mvc\Exception;
use Laminas\Mvc\MvcEvent;
use Laminas\View\Model\ViewModel;

/**
 * Basic action controller
 */
abstract class AbstractActionController extends AbstractController
{
    /**
     * {@inheritDoc}
     */
    protected $eventIdentifier = __CLASS__;

    /**
     * Default action if none provided
     *
     * @return array
     */
    public function indexAction()
    {
        return new ViewModel([
            'content' => 'Placeholder page'
        ]);
    }

    /**
     * Action called if matched action does not exist
     *
     * @return array
     */
    public function notFoundAction()
    {
        $response   = $this->response;
        $event      = $this->getEvent();
        $routeMatch = $event->getRouteMatch();
        $routeMatch->setParam('action', 'not-found');

        if ($response instanceof HttpResponse) {
            return $this->createHttpNotFoundModel($response);
        }
        return $this->createConsoleNotFoundModel($response);
    }

    /**
     * Execute the request
     *
     * @param  MvcEvent $e
     * @return mixed
     * @throws Exception\DomainException
     */
    public function onDispatch(MvcEvent $e)
    {
        $routeMatch = $e->getRouteMatch();
        if (!$routeMatch) {
            /**
             * @todo Determine requirements for when route match is missing.
             *       Potentially allow pulling directly from request metadata?
             */
            throw new Exception\DomainException('Missing route matches; unsure how to retrieve action');
        }

        $action = $routeMatch->getParam('action', 'not-found');
        $method = static::getMethodFromAction($action);

        if (!method_exists($this, $method)) {
            $method = 'notFoundAction';
        }

        $actionResponse = $this->$method();

        $e->setResult($actionResponse);

        return $actionResponse;
    }

    /**
     * @deprecated please use the {@see \Laminas\Mvc\Controller\Plugin\CreateHttpNotFoundModel} plugin instead: this
     *             method will be removed in release 2.5 or later.
     *
     * {@inheritDoc}
     */
    protected function createHttpNotFoundModel(HttpResponse $response)
    {
        return $this->__call('createHttpNotFoundModel', [$response]);
    }

    /**
     * @deprecated please use the {@see \Laminas\Mvc\Controller\Plugin\CreateConsoleNotFoundModel} plugin instead: this
     *             method will be removed in release 2.5 or later.
     *
     * {@inheritDoc}
     */
    protected function createConsoleNotFoundModel($response)
    {
        return $this->__call('createConsoleNotFoundModel', [$response]);
    }
}
