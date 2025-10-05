<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use Composer\Composer;
use Composer\EventDispatcher\EventSubscriberInterface;
use Composer\Installer\InstallerEvent;
use Composer\Installer\InstallerEvents;
use Composer\Installer\PackageEvent;
use Composer\Installer\PackageEvents;
use Composer\IO\IOInterface;
use Composer\Plugin\PluginEvents;
use Composer\Plugin\PluginInterface;
use Composer\Plugin\PreCommandRunEvent;
use Composer\Plugin\PrePoolCreateEvent;
use Composer\Script\Event;
use Composer\Script\ScriptEvents;

use function assert;
use function version_compare;

class DependencyRewriterPluginDelegator implements EventSubscriberInterface, PluginInterface
{
    /** @var RewriterInterface */
    private $rewriter;

    public function __construct(?RewriterInterface $rewriter = null)
    {
        $this->rewriter = $rewriter
            ?: $this->createDependencyRewriterForPluginVersion(PluginInterface::PLUGIN_API_VERSION);
    }

    /**
     * @return array Returns in following format:
     *     <string> => array<string, int>
     */
    public static function getSubscribedEvents()
    {
        if (version_compare(PluginInterface::PLUGIN_API_VERSION, '2.0', 'lt')) {
            /** @psalm-suppress UndefinedConstant,MixedArrayOffset */
            return [
                InstallerEvents::PRE_DEPENDENCIES_SOLVING => ['onPreDependenciesSolving', 1000],
                PackageEvents::PRE_PACKAGE_INSTALL        => ['onPrePackageInstallOrUpdate', 1000],
                PackageEvents::PRE_PACKAGE_UPDATE         => ['onPrePackageInstallOrUpdate', 1000],
                PluginEvents::PRE_COMMAND_RUN             => ['onPreCommandRun', 1000],
            ];
        }

        return [
            PluginEvents::PRE_POOL_CREATE      => ['onPrePoolCreate', 1000],
            PackageEvents::PRE_PACKAGE_INSTALL => ['onPrePackageInstallOrUpdate', 1000],
            PackageEvents::PRE_PACKAGE_UPDATE  => ['onPrePackageInstallOrUpdate', 1000],
            PluginEvents::PRE_COMMAND_RUN      => ['onPreCommandRun', 1000],
            ScriptEvents::POST_AUTOLOAD_DUMP   => ['onPostAutoloadDump', -1000],
        ];
    }

    /**
     * @return void
     */
    public function onPreDependenciesSolving(InstallerEvent $event)
    {
        $rewriter = $this->rewriter;
        assert($rewriter instanceof DependencySolvingCapableInterface);
        $rewriter->onPreDependenciesSolving($event);
    }

    /**
     * @return void
     */
    public function onPrePackageInstallOrUpdate(PackageEvent $event)
    {
        $this->rewriter->onPrePackageInstallOrUpdate($event);
    }

    /**
     * @return void
     */
    public function onPreCommandRun(PreCommandRunEvent $event)
    {
        $this->rewriter->onPreCommandRun($event);
    }

    /**
     * @return void
     */
    public function onPrePoolCreate(PrePoolCreateEvent $event)
    {
        $rewriter = $this->rewriter;
        assert($rewriter instanceof PoolCapableInterface);
        $rewriter->onPrePoolCreate($event);
    }

    /**
     * @return void
     */
    public function onPostAutoloadDump(Event $event)
    {
        $rewriter = $this->rewriter;
        assert($rewriter instanceof AutoloadDumpCapableInterface);
        $rewriter->onPostAutoloadDump($event);
    }

    /**
     * @return void
     */
    public function activate(Composer $composer, IOInterface $io)
    {
        $this->rewriter->activate($composer, $io);
    }

    /**
     * @return void
     */
    public function deactivate(Composer $composer, IOInterface $io)
    {
    }

    /**
     * @return void
     */
    public function uninstall(Composer $composer, IOInterface $io)
    {
    }

    /**
     * @param string $pluginApiVersion
     * @return DependencyRewriterV1|DependencyRewriterV2
     */
    private function createDependencyRewriterForPluginVersion($pluginApiVersion)
    {
        if (version_compare($pluginApiVersion, '2.0', 'lt')) {
            return new DependencyRewriterV1();
        }

        return new DependencyRewriterV2();
    }
}
