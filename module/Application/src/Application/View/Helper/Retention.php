<?php

/**
 * namespace definition and usage
 */
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class Retention extends AbstractHelper
{
    protected $retention;

    public function __invoke($retention)
    {
	$this->retention = (int) $retention;
	$this->retention = round(($this->retention / 60 / 60 / 24 ), 2, PHP_ROUND_HALF_EVEN);
	return $this->retention;
    }
}