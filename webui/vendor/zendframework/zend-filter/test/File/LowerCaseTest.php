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
use Zend\Filter\File\LowerCase as FileLowerCase;

class LowerCaseTest extends TestCase
{
    protected $testDir;

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
        $source = dirname(__DIR__) . '/_files/testfile2.txt';
        $this->testDir = sys_get_temp_dir();
        $this->testFile = sprintf('%s/%s.txt', $this->testDir, uniqid('zfilter'));
        copy($source, $this->testFile);
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
        $filter = new FileLowerCase();
        $filter($this->testFile);
        $this->assertContains('this is a file', file_get_contents($this->testFile));
    }

    /**
     * @return void
     */
    public function testNormalWorkflowWithFilesArray()
    {
        $this->assertContains('This is a File', file_get_contents($this->testFile));
        $filter = new FileLowerCase();
        $filter(['tmp_name' => $this->testFile]);
        $this->assertContains('this is a file', file_get_contents($this->testFile));
    }

    /**
     * @return void
     */
    public function testFileNotFoundException()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('not found');
        $filter = new FileLowerCase();
        $filter($this->testFile . 'unknown');
    }

    /**
     * @return void
     */
    public function testCheckSettingOfEncodingInIstance()
    {
        $this->assertContains('This is a File', file_get_contents($this->testFile));
        try {
            $filter = new FileLowerCase('ISO-8859-1');
            $filter($this->testFile);
            $this->assertContains('this is a file', file_get_contents($this->testFile));
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
            $filter = new FileLowerCase();
            $filter->setEncoding('ISO-8859-1');
            $filter($this->testFile);
            $this->assertContains('this is a file', file_get_contents($this->testFile));
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
                sprintf('%s/%s.txt', $this->testDir, uniqid()),
                sprintf('%s/%s.txt', $this->testDir, uniqid()),
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new FileLowerCase();

        $this->assertEquals($input, $filter($input));
    }
}
