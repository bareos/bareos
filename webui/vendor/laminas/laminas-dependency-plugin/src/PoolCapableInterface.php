<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use Composer\Plugin\PrePoolCreateEvent;

interface PoolCapableInterface extends RewriterInterface
{
    /**
     * If a ZF package is being installed, ensure the pool is modified to install the laminas equivalent instead.
     *
     * @return void
     */
    public function onPrePoolCreate(PrePoolCreateEvent $event);
}
