<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Exception\RuntimeException;
use Zend\Filter\FilterPluginManager;
use Zend\Filter\Word\SeparatorToSeparator;
use Zend\ServiceManager\Exception\InvalidServiceException;
use Zend\ServiceManager\ServiceManager;

class FilterPluginManagerTest extends TestCase
{
    /**
     * @var FilterPluginManager
     */
    private $filters;

    public function setUp()
    {
        $this->filters = new FilterPluginManager(new ServiceManager());
    }

    public function testFilterSuccessfullyRetrieved()
    {
        $filter = $this->filters->get('int');
        $this->assertInstanceOf('Zend\Filter\ToInt', $filter);
    }

    public function testRegisteringInvalidFilterRaisesException()
    {
        $this->expectException($this->getInvalidServiceException());
        $this->filters->setService('test', $this);
        $this->filters->get('test');
    }

    public function testLoadingInvalidFilterRaisesException()
    {
        $this->filters->setInvokableClass('test', get_class($this));
        $this->expectException($this->getInvalidServiceException());
        $this->filters->get('test');
    }

    /**
     * @group 7169
     */
    public function testFilterSuccessfullyConstructed()
    {
        $searchSeparator      = ';';
        $replacementSeparator = '|';

        $options = [
            'search_separator'      => $searchSeparator,
            'replacement_separator' => $replacementSeparator,
        ];

        $filter = $this->filters->get('wordseparatortoseparator', $options);

        $this->assertInstanceOf(SeparatorToSeparator::class, $filter);
        $this->assertEquals($searchSeparator, $filter->getSearchSeparator());
        $this->assertEquals($replacementSeparator, $filter->getReplacementSeparator());
    }

    /**
     * @group 7169
     */
    public function testFiltersConstructedAreDifferent()
    {
        $filterOne = $this->filters->get(
            'wordseparatortoseparator',
            [
                'search_separator'      => ';',
                'replacement_separator' => '|',
            ]
        );

        $filterTwo = $this->filters->get(
            'wordseparatortoseparator',
            [
                'search_separator'      => '.',
                'replacement_separator' => ',',
            ]
        );

        $this->assertNotEquals($filterOne, $filterTwo);
    }


    protected function getInvalidServiceException()
    {
        if (method_exists($this->filters, 'configure')) {
            return InvalidServiceException::class;
        }
        return RuntimeException::class;
    }
}
