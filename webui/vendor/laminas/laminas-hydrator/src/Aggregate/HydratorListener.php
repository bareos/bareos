<?php

/**
 * @see       https://github.com/laminas/laminas-hydrator for the canonical source repository
 * @copyright https://github.com/laminas/laminas-hydrator/blob/master/COPYRIGHT.md
 * @license   https://github.com/laminas/laminas-hydrator/blob/master/LICENSE.md New BSD License
 */

namespace Laminas\Hydrator\Aggregate;

use Laminas\EventManager\AbstractListenerAggregate;
use Laminas\EventManager\EventManagerInterface;
use Laminas\Hydrator\HydratorInterface;

/**
 * Aggregate listener wrapping around a hydrator.
 *
 * Listens to {@see HydrateEvent::EVENT_HYDRATE} and {@see ExtractEvent::EVENT_EXTRACT}
 */
class HydratorListener extends AbstractListenerAggregate
{
    /**
     * @var HydratorInterface
     */
    protected $hydrator;

    /**
     * @param HydratorInterface $hydrator
     */
    public function __construct(HydratorInterface $hydrator)
    {
        $this->hydrator = $hydrator;
    }

    /**
     * {@inheritDoc}
     */
    public function attach(EventManagerInterface $events, $priority = 1)
    {
        $this->listeners[] = $events->attach(HydrateEvent::EVENT_HYDRATE, [$this, 'onHydrate'], $priority);
        $this->listeners[] = $events->attach(ExtractEvent::EVENT_EXTRACT, [$this, 'onExtract'], $priority);
    }

    /**
     * Callback to be used when {@see HydrateEvent::EVENT_HYDRATE} is triggered
     *
     * @param HydrateEvent $event
     * @return object
     * @internal
     */
    public function onHydrate(HydrateEvent $event)
    {
        $object = $this->hydrator->hydrate($event->getHydrationData(), $event->getHydratedObject());
        $event->setHydratedObject($object);
        return $object;
    }

    /**
     * Callback to be used when {@see ExtractEvent::EVENT_EXTRACT} is triggered
     *
     * @param ExtractEvent $event
     * @return array
     * @internal
     */
    public function onExtract(ExtractEvent $event)
    {
        $data = $this->hydrator->extract($event->getExtractionObject());
        $event->mergeExtractedData($data);
        return $data;
    }
}
