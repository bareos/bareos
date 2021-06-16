<?php
/**
 * @see       https://github.com/zendframework/zend-inputfilter for the canonical source repository
 * @copyright Copyright (c) 2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-inputfilter/blob/master/LICENSE.md New BSD License
 */

namespace Zend\InputFilter;

/**
 * Ensures Inputs store unfiltered data and are capable of returning it
 */
interface UnfilteredDataInterface
{
    /**
     * @return array|object
     */
    public function getUnfilteredData();

    /**
     * @param array|object $data
     * @return $this
     */
    public function setUnfilteredData($data);
}
