<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use Composer\Composer;
use Composer\Console\Application;
use Composer\DependencyResolver\Operation;
use Composer\Factory;
use Composer\Installer\PackageEvent;
use Composer\IO\IOInterface;
use Composer\Json\JsonFile;
use Composer\Package\PackageInterface;
use Composer\Plugin\PrePoolCreateEvent;
use Composer\Repository\InstalledFilesystemRepository;
use Composer\Script\Event;
use Symfony\Component\Console\Input\ArgvInput;
use Symfony\Component\Console\Input\ArrayInput;
use Symfony\Component\Console\Input\InputInterface;

use function array_merge;
use function assert;
use function call_user_func;
use function dirname;
use function get_class;
use function in_array;
use function is_array;
use function ksort;
use function sprintf;

/** @psalm-suppress PropertyNotSetInConstructor */
final class DependencyRewriterV2 extends AbstractDependencyRewriter implements
    PoolCapableInterface,
    AutoloadDumpCapableInterface
{
    public const COMPOSER_LOCK_UPDATE_OPTIONS = [
        'ignore-platform-reqs',
        'ignore-platform-req',
    ];

    /** @var PackageInterface[] */
    public $zendPackagesInstalled = [];

    /** @var callable */
    private $applicationFactory;

    /** @var string */
    private $composerFile;

    /** @var InputInterface */
    private $input;

    public function __construct(
        ?callable $applicationFactory = null,
        string $composerFile = '',
        ?InputInterface $input = null
    ) {
        parent::__construct();

        /** @psalm-suppress MixedAssignment */
        $this->composerFile       = $composerFile ?: Factory::getComposerFile();
        $this->applicationFactory = $applicationFactory ?? static function (): Application {
            return new Application();
        };
        $this->input              = $input ?? new ArgvInput();
    }

    /**
     * Ensure nested dependencies on ZF packages install equivalent Laminas packages.
     *
     * When a 3rd party package has dependencies on ZF packages, this method
     * will detect the request to install a ZF package, and rewrite it to use a
     * Laminas variant at the equivalent version, if one exists.
     *
     * @return void
     */
    public function onPrePackageInstallOrUpdate(PackageEvent $event)
    {
        $this->output(sprintf('<info>In %s</info>', __METHOD__), IOInterface::DEBUG);
        $operation = $event->getOperation();

        switch (true) {
            case $operation instanceof Operation\InstallOperation:
                $package = $operation->getPackage();
                break;
            case $operation instanceof Operation\UpdateOperation:
                $package = $operation->getTargetPackage();
                break;
            default:
                // Nothing to do
                $this->output(sprintf(
                    '<info>Exiting; operation of type %s not supported</info>',
                    get_class($operation)
                ), IOInterface::DEBUG);
                return;
        }

        $name = $package->getName();
        if (! $this->isZendPackage($name)) {
            // Nothing to do
            $this->output(sprintf(
                '<info>Exiting; package "%s" does not have a replacement</info>',
                $name
            ), IOInterface::DEBUG);
            return;
        }

        $replacementName = $this->transformPackageName($name);
        if ($replacementName === $name) {
            // Nothing to do
            $this->output(sprintf(
                '<info>Exiting; while package "%s" is a ZF package, it does not have a replacement</info>',
                $name
            ), IOInterface::DEBUG);
            return;
        }

        $version            = $package->getVersion();
        $repositoryManager  = $this->composer->getRepositoryManager();
        $replacementPackage = $repositoryManager->findPackage($replacementName, $version);

        if ($replacementPackage === null) {
            // No matching replacement package found
            $this->output(sprintf(
                '<info>Exiting; no replacement package found for package "%s" with version %s</info>',
                $replacementName,
                $version
            ), IOInterface::DEBUG);
            return;
        }

        $this->output(sprintf(
            '<info>Could replace package %s with package %s, using version %s</info>',
            $name,
            $replacementName,
            $version
        ), IOInterface::VERBOSE);

        $this->zendPackagesInstalled[] = $package;
    }

    /**
     * @return void
     */
    public function onPostAutoloadDump(Event $event)
    {
        if (! $this->zendPackagesInstalled) {
            return;
        }

        // Remove zend-packages from vendor/ directory
        $composer   = $event->getComposer();
        $installers = $composer->getInstallationManager();
        $repository = $composer->getRepositoryManager()->getLocalRepository();

        $composerFile = $this->createComposerFile();
        $definition   = $composerFile->read();
        assert(is_array($definition));
        $definitionChanged = false;

        foreach ($this->zendPackagesInstalled as $package) {
            $packageName     = $package->getName();
            $replacementName = $this->transformPackageName($packageName);
            if ($this->isRootRequirement($definition, $packageName)) {
                $this->output(sprintf(
                    '<info>Package %s is a root requirement. laminas-dependency-plugin changes your composer.json'
                    . ' to require laminas equivalent directly!</info>',
                    $packageName
                ));

                $definitionChanged = true;
                $definition        = $this->updateRootRequirements(
                    $definition,
                    $packageName,
                    $replacementName
                );
            }

            $uninstallOperation = new Operation\UninstallOperation($package);
            $installers->uninstall($repository, $uninstallOperation);
        }

        if ($definitionChanged) {
            $composerFile->write($definition);
        }

        $this->updateLockFile();
    }

    /**
     * @return void
     */
    public function onPrePoolCreate(PrePoolCreateEvent $event)
    {
        $this->output(sprintf('In %s', __METHOD__));

        $installedRepository = $this->createInstalledRepository($this->composer, $this->io);
        $installedPackages   = $installedRepository->getPackages();

        $installedZendPackages = [];

        foreach ($installedPackages as $package) {
            if (! $this->isZendPackage($package->getName())) {
                continue;
            }

            $installedZendPackages[] = $package->getName();
        }

        if (! $installedZendPackages) {
            return;
        }

        $unacceptableFixedPackages = $event->getUnacceptableFixedPackages();
        $repository                = $this->composer->getRepositoryManager();
        $packages                  = $event->getPackages();

        foreach ($packages as $index => $package) {
            if (! in_array($package->getName(), $installedZendPackages, true)) {
                continue;
            }

            $replacement = $this->transformPackageName($package->getName());
            if ($replacement === $package->getName()) {
                continue;
            }

            $replacementPackage = $repository->findPackage($replacement, $package->getVersion());
            if (! $replacementPackage instanceof PackageInterface) {
                continue;
            }

            $unacceptableFixedPackages[] = $package;

            $this->output(sprintf('Slipstreaming %s => %s', $package->getName(), $replacement));
            $packages[$index] = $replacementPackage;
        }

        $event->setUnacceptableFixedPackages($unacceptableFixedPackages);
        $event->setPackages($packages);
    }

    /**
     * With `composer update --lock`, all missing packages are being installed aswell.
     * This is where we slip-stream in with our plugin.
     */
    private function updateLockFile(): void
    {
        $application = call_user_func($this->applicationFactory);
        assert($application instanceof Application);

        $application->setAutoExit(false);

        $input = [
            'command'       => 'update',
            '--lock'        => true,
            '--no-scripts'  => true,
            '--working-dir' => dirname($this->composerFile),
        ];

        $input = array_merge($input, $this->extractAdditionalInputOptionsFromInput(
            $this->input,
            self::COMPOSER_LOCK_UPDATE_OPTIONS
        ));

        $application->run(new ArrayInput($input));
    }

    /**
     * @return InstalledFilesystemRepository
     */
    private function createInstalledRepository(Composer $composer, IOInterface $io)
    {
        /** @var string $vendor */
        $vendor = $composer->getConfig()->get('vendor-dir');

        return new InstalledFilesystemRepository(
            new JsonFile($vendor . '/composer/installed.json', null, $io),
            true,
            $composer->getPackage()
        );
    }

    /**
     * @param string $packageName
     * @return bool
     */
    private function isRootRequirement(array $definition, $packageName)
    {
        return isset($definition['require'][$packageName]) || isset($definition['require-dev'][$packageName]);
    }

    /**
     * @param string $packageName
     * @param string $replacementPackageName
     * @return array
     */
    private function updateRootRequirements(array $definition, $packageName, $replacementPackageName)
    {
        /** @var bool $sortPackages */
        $sortPackages = $definition['config']['sort-packages'] ?? false;

        foreach (['require', 'require-dev'] as $key) {
            if (! isset($definition[$key])) {
                continue;
            }

            /** @var array $requirements */
            $requirements = $definition[$key];
            if (! isset($requirements[$packageName])) {
                continue;
            }

            /** @psalm-suppress MixedAssignment */
            $requirements[$replacementPackageName] = $requirements[$packageName];
            unset($requirements[$packageName]);
            if ($sortPackages) {
                ksort($requirements);
            }

            $definition[$key] = $requirements;
        }

        return $definition;
    }

    /**
     * @deprecated Please use public property {@see DependencyRewriterV2::$zendPackagesInstalled} instead.
     *
     * @return PackageInterface[]
     */
    public function getZendPackagesInstalled()
    {
        return $this->zendPackagesInstalled;
    }

    /**
     * @return JsonFile
     */
    private function createComposerFile()
    {
        return new JsonFile($this->composerFile, null, $this->io);
    }

    /**
     * @psalm-param list<non-empty-string> $options
     * @psalm-return array<non-empty-string,mixed>
     */
    private function extractAdditionalInputOptionsFromInput(InputInterface $input, array $options): array
    {
        $additionalInputOptions = [];
        foreach ($options as $optionName) {
            $option = sprintf('--%s', $optionName);
            assert(! empty($option));
            if (! $input->hasParameterOption($option, true)) {
                continue;
            }
            /** @psalm-suppress MixedAssignment */
            $additionalInputOptions[$option] = $input->getParameterOption($option, false, true);
        }

        return $additionalInputOptions;
    }
}
