<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use Composer\Installer\InstallerEvent;

interface DependencySolvingCapableInterface extends RewriterInterface
{
    /**
     * If a ZF package is being installed, modify the incoming request to slip-stream laminas packages.
     *
     * @return void
     */
    public function onPreDependenciesSolving(InstallerEvent $event);
}
