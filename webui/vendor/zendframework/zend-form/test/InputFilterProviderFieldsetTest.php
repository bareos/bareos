<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form;

use PHPUnit\Framework\TestCase;
use Zend\Form\InputFilterProviderFieldset;

class InputFilterProviderFieldsetTest extends TestCase
{
    public function setUp()
    {
        $this->fieldset = new InputFilterProviderFieldset();
    }

    public function testCanSetInputFilterSpec()
    {
        $filterSpec = ['filter' => ['filter_options']];

        $this->fieldset->setInputFilterSpecification($filterSpec);
        $this->assertEquals($filterSpec, $this->fieldset->getInputFilterSpecification());
    }

    public function testCanSetInputFilterSpecViaOptions()
    {
        $filterSpec = ['filter' => ['filter_options']];

        $this->fieldset->setOptions(['input_filter_spec' => $filterSpec]);
        $this->assertEquals($filterSpec, $this->fieldset->getInputFilterSpecification());
    }

    public function testFilterSpecIsInitiallyEmpty()
    {
        $this->assertEmpty($this->fieldset->getInputFilterSpecification());
    }
}
