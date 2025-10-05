<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use Composer\Composer;
use Composer\Installer\PackageEvent;
use Composer\IO\IOInterface;
use Composer\Plugin\PreCommandRunEvent;

interface RewriterInterface
{
    /**
     * @return void
     */
    public function activate(Composer $composer, IOInterface $io);

    /**
     * When a ZF package is requested, replace with the Laminas variant.
     *
     * When a `require` operation is requested, and a ZF package is detected,
     * this listener will replace the argument with the equivalent Laminas
     * package. This ensures that the `composer.json` file is written to
     * reflect the package installed.
     *
     * @return void
     */
    public function onPreCommandRun(PreCommandRunEvent $event);

    /**
     * When a ZF package is installed or updated, ensure that its being replaced after installation/update is finished.
     *
     * @return void
     */
    public function onPrePackageInstallOrUpdate(PackageEvent $event);
}
