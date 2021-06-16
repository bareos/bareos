<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Model;

use PHPUnit\Framework\TestCase;
use Zend\View\Model\JsonModel;
use Zend\Json\Json;
use Zend\View\Variables;

class JsonModelTest extends TestCase
{
    public function testAllowsEmptyConstructor()
    {
        $model = new JsonModel();
        $this->assertInstanceOf(Variables::class, $model->getVariables());
        $this->assertEquals([], $model->getOptions());
    }

    public function testCanSerializeVariablesToJson()
    {
        $array = ['foo' => 'bar'];
        $model = new JsonModel($array);
        $this->assertEquals($array, $model->getVariables());
        $this->assertEquals(Json::encode($array), $model->serialize());
    }

    public function testCanSerializeWithJsonpCallback()
    {
        $array = ['foo' => 'bar'];
        $model = new JsonModel($array);
        $model->setJsonpCallback('callback');
        $this->assertEquals('callback(' . Json::encode($array) . ');', $model->serialize());
    }

    public function testPrettyPrint()
    {
        $array = [
            'simple' => 'simple test string',
            'stringwithjsonchars' => '\"[1,2]',
            'complex' => [
                'foo' => 'bar',
                'far' => 'boo'
            ]
        ];
        $model = new JsonModel($array, ['prettyPrint' => true]);
        $this->assertEquals(Json::encode($array, false, ['prettyPrint' => true]), $model->serialize());
    }
}
