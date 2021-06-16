<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace Zend\Hydrator\Aggregate;

use Zend\EventManager\EventManager;
use Zend\EventManager\EventManagerAwareInterface;
use Zend\EventManager\EventManagerInterface;
use Zend\Hydrator\HydratorInterface;

/**
 * Aggregate hydrator that composes multiple hydrators via events
 */
class AggregateHydrator implements HydratorInterface, EventManagerAwareInterface
{
    const DEFAULT_PRIORITY = 1;

    /**
     * @var EventManagerInterface|null
     */
    protected $eventManager;

    /**
     * Attaches the provided hydrator to the list of hydrators to be used while hydrating/extracting data
     *
     * @param HydratorInterface $hydrator
     * @param int $priority
     */
    public function add(HydratorInterface $hydrator, $priority = self::DEFAULT_PRIORITY)
    {
        $listener = new HydratorListener($hydrator);
        $listener->attach($this->getEventManager(), $priority);
    }

    /**
     * {@inheritDoc}
     */
    public function extract($object)
    {
        $event = new ExtractEvent($this, $object);

        $this->getEventManager()->triggerEvent($event);

        return $event->getExtractedData();
    }

    /**
     * {@inheritDoc}
     */
    public function hydrate(array $data, $object)
    {
        $event = new HydrateEvent($this, $object, $data);

        $this->getEventManager()->triggerEvent($event);

        return $event->getHydratedObject();
    }

    /**
     * {@inheritDoc}
     */
    public function setEventManager(EventManagerInterface $eventManager)
    {
        $eventManager->setIdentifiers([__CLASS__, get_class($this)]);

        $this->eventManager = $eventManager;
    }

    /**
     * {@inheritDoc}
     */
    public function getEventManager()
    {
        if (null === $this->eventManager) {
            $this->setEventManager(new EventManager());
        }

        return $this->eventManager;
    }
}
