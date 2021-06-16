<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Cache\Storage\TestAsset;

use Zend\Cache\Storage\Plugin;
use Zend\Cache\Storage\Plugin\AbstractPlugin;
use Zend\EventManager\EventManagerInterface;
use Zend\EventManager\Event;

class MockPlugin extends AbstractPlugin
{

    protected $options;
    protected $handles = [];
    protected $calledEvents = [];
    protected $eventCallbacks  = [
        'setItem.pre'  => 'onSetItemPre',
        'setItem.post' => 'onSetItemPost'
    ];

    public function __construct($options = [])
    {
        if (is_array($options)) {
            $options = new Plugin\PluginOptions($options);
        }
        if ($options instanceof Plugin\PluginOptions) {
            $this->setOptions($options);
        }
    }

    public function setOptions(Plugin\PluginOptions $options)
    {
        $this->options = $options;
        return $this;
    }

    public function getOptions()
    {
        return $this->options;
    }

    public function attach(EventManagerInterface $eventCollection, $priority = 1)
    {
        foreach ($this->eventCallbacks as $eventName => $method) {
            $this->listeners[] = $eventCollection->attach($eventName, [$this, $method], $priority);
        }
    }

    public function onSetItemPre(Event $event)
    {
        $this->calledEvents[] = $event;
    }

    public function onSetItemPost(Event $event)
    {
        $this->calledEvents[] = $event;
    }

    public function getHandles()
    {
        return $this->listeners;
    }

    public function getEventCallbacks()
    {
        return $this->eventCallbacks;
    }

    public function getCalledEvents()
    {
        return $this->calledEvents;
    }
}
