<?php
/**
 * @see       https://github.com/zendframework/zend-view for the canonical source repository
 * @copyright Copyright (c) 2018 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-view/blob/master/LICENSE.md New BSD License
 */

namespace ZendTest\View\Model;

use PHPUnit\Framework\TestCase;
use Zend\View\Model\ConsoleModel;
use Zend\View\Model\ModelInterface;

class ConsoleModelTest extends TestCase
{
    public function testImplementsModelInterface()
    {
        $model = new ConsoleModel();
        $this->assertInstanceOf(ModelInterface::class, $model);
    }

    /**
     * @see https://github.com/zendframework/zend-view/issues/152
     */
    public function testSetErrorLevelImplementsFluentInterface()
    {
        $model = new ConsoleModel();
        $actual = $model->setErrorLevel(0);
        $this->assertSame($model, $actual);
    }
}
