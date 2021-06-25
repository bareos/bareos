<?php
/**
 * @link      https://github.com/zendframework/zend-modulemanager for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-modulemanager/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\ModuleManager\TestAsset;

use Zend\EventManager\EventManagerInterface;
use Zend\Mvc\ApplicationInterface;
use Zend\Mvc\MvcEvent;

class MockApplication implements ApplicationInterface
{
    public $events;
    public $request;
    public $response;
    public $serviceManager;

    public function setEventManager(EventManagerInterface $events)
    {
        $this->events = $events;
    }

    /**
     * {@inheritDoc}
     */
    public function getEventManager()
    {
        return $this->events;
    }

    /**
     * {@inheritDoc}
     */
    public function getServiceManager()
    {
        return $this->serviceManager;
    }

    public function setServiceManager($serviceManager)
    {
        $this->serviceManager = $serviceManager;
        return $this;
    }

    /**
     * {@inheritDoc}
     */
    public function getRequest()
    {
        return $this->request;
    }

    /**
     * {@inheritDoc}
     */
    public function getResponse()
    {
        return $this->response;
    }

    /**
     * {@inheritDoc}
     */
    public function run()
    {
        return $this->response;
    }

    public function bootstrap()
    {
        $event = new MvcEvent();
        $event->setApplication($this);
        $event->setTarget($this);
        $event->setName(MvcEvent::EVENT_BOOTSTRAP);
        $this->getEventManager()->triggerEvent($event);
    }
}
