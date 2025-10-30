<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use Composer\Composer;
use Composer\Installer\PackageEvent;
use Composer\IO\IOInterface;
use Composer\Plugin\PreCommandRunEvent;

use function array_map;
use function array_shift;
use function in_array;
use function is_array;
use function preg_split;
use function reset;
use function sprintf;

/** @psalm-suppress PropertyNotSetInConstructor */
abstract class AbstractDependencyRewriter implements RewriterInterface
{
    /**
     * @psalm-suppress PropertyNotSetInConstructor
     * @var Composer
     */
    protected $composer;

    /**
     * @psalm-suppress PropertyNotSetInConstructor
     * @var IOInterface
     */
    protected $io;

    /** @var Replacements */
    private $replacements;

    public function __construct()
    {
        $this->replacements = new Replacements();
    }

    /**
     * @return void
     */
    public function activate(Composer $composer, IOInterface $io)
    {
        $this->composer = $composer;
        $this->io       = $io;
        $this->output(sprintf('<info>Activating %s</info>', static::class), IOInterface::DEBUG);
    }

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
    public function onPreCommandRun(PreCommandRunEvent $event)
    {
        $this->output(
            sprintf(
                '<info>In %s::%s</info>',
                static::class,
                __FUNCTION__
            ),
            IOInterface::DEBUG
        );

        if (! in_array($event->getCommand(), ['require', 'update'], true)) {
            // Nothing to do here.
            return;
        }

        $input = $event->getInput();
        if (! $input->hasArgument('packages')) {
            return;
        }

        /** @psalm-var null|array<array-key, string|numeric> $packages */
        $packages = $input->getArgument('packages');

        // Ensure we have an array of strings
        $packages = is_array($packages) ? $packages : [];
        $packages = array_map(
            /** @param scalar $value */
            static function ($value): string {
                return (string) $value;
            },
            $packages
        );

        $input->setArgument(
            'packages',
            array_map([$this, 'updatePackageArgument'], $packages)
        );
    }

    abstract public function onPrePackageInstallOrUpdate(PackageEvent $event);

    /**
     * @param string $message
     * @param int $verbosity
     */
    protected function output($message, $verbosity = IOInterface::NORMAL): void
    {
        $this->io->write($message, true, $verbosity);
    }

    /**
     * Parses a package argument from the command line, replacing it with the
     * Laminas variant if it exists.
     *
     * @param string $package Package specification from command line
     * @return string Modified package specification containing Laminas
     *     substitution, or original if no changes required.
     */
    private function updatePackageArgument($package)
    {
        $result = preg_split('/[ :=]/', $package, 2);
        if ($result === false) {
            return $package;
        }
        $name = array_shift($result);

        if (! $this->isZendPackage($name)) {
            return $package;
        }

        $replacementName = $this->transformPackageName($name);
        if ($replacementName === $name) {
            return $package;
        }

        $this->io->write(
            sprintf(
                'Changing package in current command from %s to %s',
                $name,
                $replacementName
            ),
            true,
            IOInterface::DEBUG
        );

        $version = reset($result);

        if ($version === false) {
            return $replacementName;
        }

        return sprintf('%s:%s', $replacementName, $version);
    }

    /**
     * @param string $name
     * @return bool
     */
    protected function isZendPackage($name)
    {
        return $this->replacements->isZendPackage($name);
    }

    /**
     * @param string $name
     * @return string
     */
    protected function transformPackageName($name)
    {
        return $this->replacements->transformPackageName($name);
    }
}
