<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2016 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Form\Element;

use PHPUnit\Framework\TestCase;
use Zend\Form\Element\Search;

class SearchTest extends TestCase
{
    public function testType()
    {
        $element = new Search('test');

        $this->assertSame('search', $element->getAttribute('type'));
    }
}
