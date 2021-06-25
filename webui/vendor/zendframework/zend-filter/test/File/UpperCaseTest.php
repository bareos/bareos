<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\Filter\File;

use PHPUnit\Framework\TestCase;
use Zend\Filter\Exception;
use Zend\Filter\File\UpperCase as FileUpperCase;

class UpperCaseTest extends TestCase
{
    /**
     * Testfile
     *
     * @var string
     */
    protected $testFile;

    /**
     * Sets the path to test files
     *
     * @return void
     */
    public function setUp()
    {
        $filesPath = dirname(__DIR__) . DIRECTORY_SEPARATOR . '_files' . DIRECTORY_SEPARATOR;
        $origFile  = $filesPath . 'testfile2.txt';
        $this->testFile = sprintf('%s/%s.txt', sys_get_temp_dir(), uniqid('zfilter'));

        copy($origFile, $this->testFile);
    }

    /**
     * Sets the path to test files
     *
     * @return void
     */
    public function tearDown()
    {
        if (file_exists($this->testFile)) {
            unlink($this->testFile);
        }
    }

    /**
     * @return void
     */
    public function testInstanceCreationAndNormalWorkflow()
    {
        $this->assertContains('This is a File', file_get_contents($this->testFile));
        $filter = new FileUpperCase();
        $filter($this->testFile);
        $this->assertContains('THIS IS A FILE', file_get_contents($this->testFile));
    }

    /**
     * @return void
     */
    public function testNormalWorkflowWithFilesArray()
    {
        $this->assertContains('This is a File', file_get_contents($this->testFile));
        $filter = new FileUpperCase();
        $filter(['tmp_name' => $this->testFile]);
        $this->assertContains('THIS IS A FILE', file_get_contents($this->testFile));
    }

    /**
     * @return void
     */
    public function testFileNotFoundException()
    {
        $filter = new FileUpperCase();
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('not found');
        $filter($this->testFile . 'unknown');
    }

    /**
     * @return void
     */
    public function testCheckSettingOfEncodingInIstance()
    {
        $this->assertContains('This is a File', file_get_contents($this->testFile));
        try {
            $filter = new FileUpperCase('ISO-8859-1');
            $filter($this->testFile);
            $this->assertContains('THIS IS A FILE', file_get_contents($this->testFile));
        } catch (\Zend\Filter\Exception\ExtensionNotLoadedException $e) {
            $this->assertContains('mbstring is required', $e->getMessage());
        }
    }

    /**
     * @return void
     */
    public function testCheckSettingOfEncodingWithMethod()
    {
        $this->assertContains('This is a File', file_get_contents($this->testFile));
        try {
            $filter = new FileUpperCase();
            $filter->setEncoding('ISO-8859-1');
            $filter($this->testFile);
            $this->assertContains('THIS IS A FILE', file_get_contents($this->testFile));
        } catch (\Zend\Filter\Exception\ExtensionNotLoadedException $e) {
            $this->assertContains('mbstring is required', $e->getMessage());
        }
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                $this->testFile,
                'something invalid'
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new FileUpperCase();
        $filter->setEncoding('ISO-8859-1');

        $this->assertEquals($input, $filter($input));
    }
}
