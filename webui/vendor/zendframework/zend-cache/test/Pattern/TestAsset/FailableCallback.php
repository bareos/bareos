<?php

namespace ZendTest\Cache\Pattern\TestAsset;

class FailableCallback
{
    public function __invoke()
    {
        throw new \Exception('This callback should either fail or never be invoked');
    }
}
