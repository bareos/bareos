<?php

declare(strict_types=1);

namespace Laminas\DependencyPlugin;

use Composer\Script\Event;

interface AutoloadDumpCapableInterface extends RewriterInterface
{
    /**
     * @return void
     */
    public function onPostAutoloadDump(Event $event);
}
