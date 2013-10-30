<?php

/**
 * namespace definition and usage
 */
namespace Application\View\Helper;

use Zend\View\Helper\AbstractHelper;

class Bytes extends AbstractHelper
{
    protected $mb;

    public function __invoke($bytes)
    {
	$bytes = (int) $bytes;
	$this->mb = ( ($bytes / 1024) / 1024 );
	$this->mb = round( $this->mb, 2, PHP_ROUND_HALF_EVEN ) . " MB";
	return $this->mb;
    }
}