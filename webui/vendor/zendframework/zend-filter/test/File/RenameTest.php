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
use Zend\Filter\File\Rename as FileRename;

class RenameTest extends TestCase
{
    /**
     * Path to test files
     *
     * @var string
     */
    protected $tmpPath;

    /**
     * Original testfile
     *
     * @var string
     */
    protected $origFile;

    /**
     * Testfile
     *
     * @var string
     */
    protected $oldFile;

    /**
     * Testfile
     *
     * @var string
     */
    protected $newFile;

    /**
     * Testdirectory
     *
     * @var string
     */
    protected $newDir;

    /**
     * Testfile in Testdirectory
     *
     * @var string
     */
    protected $newDirFile;

    /**
     * Sets the path to test files
     *
     * @return void
     */
    public function setUp()
    {
        $control = sprintf('%s/_files/testfile.txt', dirname(__DIR__));
        $this->tmpPath = sprintf('%s%s%s', sys_get_temp_dir(), DIRECTORY_SEPARATOR, uniqid('zfilter'));
        mkdir($this->tmpPath, 0775, true);

        $this->oldFile = sprintf('%s%stestfile.txt', $this->tmpPath, DIRECTORY_SEPARATOR);
        $this->origFile = sprintf('%s%soriginal.file', $this->tmpPath, DIRECTORY_SEPARATOR);
        $this->newFile = sprintf('%s%snewfile.xml', $this->tmpPath, DIRECTORY_SEPARATOR);
        $this->newDir = sprintf('%s%stestdir', $this->tmpPath, DIRECTORY_SEPARATOR);
        $this->newDirFile = sprintf('%s%stestfile.txt', $this->newDir, DIRECTORY_SEPARATOR);

        copy($control, $this->oldFile);
        copy($control, $this->origFile);
        mkdir($this->newDir, 0775, true);
    }

    /**
     * Sets the path to test files
     *
     * @return void
     */
    public function tearDown()
    {
        if (is_dir($this->tmpPath)) {
            if (file_exists($this->oldFile)) {
                unlink($this->oldFile);
            }
            if (file_exists($this->origFile)) {
                unlink($this->origFile);
            }
            if (file_exists($this->newFile)) {
                unlink($this->newFile);
            }
            if (is_dir($this->newDir)) {
                if (file_exists($this->newDirFile)) {
                    unlink($this->newDirFile);
                }
                rmdir($this->newDir);
            }
            rmdir($this->tmpPath);
        }
    }

    /**
     * Test single parameter filter
     *
     * @return void
     */
    public function testConstructSingleValue()
    {
        $filter = new FileRename($this->newFile);

        $this->assertEquals(
            [0 => [
                'source'    => '*',
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single parameter filter
     *
     * @return void
     */
    public function testConstructSingleValueWithFilesArray()
    {
        $filter = new FileRename($this->newFile);

        $this->assertEquals(
            [0 => [
                'source'    => '*',
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals(
            ['tmp_name' => $this->newFile],
            $filter(['tmp_name' => $this->oldFile])
        );
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single array parameter filter
     *
     * @return void
     */
    public function testConstructSingleArray()
    {
        $filter = new FileRename([
            'source' => $this->oldFile,
            'target' => $this->newFile]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test full array parameter filter
     *
     * @return void
     */
    public function testConstructFullOptionsArray()
    {
        $filter = new FileRename([
            'source' => $this->oldFile,
            'target' => $this->newFile,
            'overwrite' => true,
            'randomize' => false,
            'unknown'   => false
        ]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newFile,
                'overwrite' => true,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single array parameter filter
     *
     * @return void
     */
    public function testConstructDoubleArray()
    {
        $filter = new FileRename([
            0 => [
                'source' => $this->oldFile,
                'target' => $this->newFile]]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single array parameter filter
     *
     * @return void
     */
    public function testConstructTruncatedTarget()
    {
        $filter = new FileRename([
            'source' => $this->oldFile]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => '*',
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->oldFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single array parameter filter
     *
     * @return void
     */
    public function testConstructTruncatedSource()
    {
        $filter = new FileRename([
            'target' => $this->newFile]);

        $this->assertEquals(
            [0 => [
                'source'    => '*',
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single parameter filter by using directory only
     *
     * @return void
     */
    public function testConstructSingleDirectory()
    {
        $filter = new FileRename($this->newDir);

        $this->assertEquals(
            [0 => [
                'source'    => '*',
                'target'    => $this->newDir,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newDirFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single array parameter filter by using directory only
     *
     * @return void
     */
    public function testConstructSingleArrayDirectory()
    {
        $filter = new FileRename([
            'source' => $this->oldFile,
            'target' => $this->newDir]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newDir,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newDirFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single array parameter filter by using directory only
     *
     * @return void
     */
    public function testConstructDoubleArrayDirectory()
    {
        $filter = new FileRename([
            0 => [
                'source' => $this->oldFile,
                'target' => $this->newDir]]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newDir,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newDirFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * Test single array parameter filter by using directory only
     *
     * @return void
     */
    public function testConstructTruncatedSourceDirectory()
    {
        $filter = new FileRename([
            'target' => $this->newDir]);

        $this->assertEquals(
            [0 => [
                'source'    => '*',
                'target'    => $this->newDir,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newDirFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @return void
     */
    public function testAddSameFileAgainAndOverwriteExistingTarget()
    {
        $filter = new FileRename([
            'source' => $this->oldFile,
            'target' => $this->newDir]);

        $filter->addFile([
            'source' => $this->oldFile,
            'target' => $this->newFile]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @return void
     */
    public function testGetNewName()
    {
        $filter = new FileRename([
            'source' => $this->oldFile,
            'target' => $this->newDir,
        ]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newDir,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newDirFile, $filter->getNewName($this->oldFile));
    }

    /**
     * @return void
     */
    public function testGetNewNameExceptionWithExistingFile()
    {
        $filter = new FileRename([
            'source' => $this->oldFile,
            'target' => $this->newFile]);

        copy($this->oldFile, $this->newFile);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('could not be renamed');
        $this->assertEquals($this->newFile, $filter->getNewName($this->oldFile));
    }

    /**
     * @return void
     */
    public function testGetNewNameOverwriteWithExistingFile()
    {
        $filter = new FileRename([
            'source'    => $this->oldFile,
            'target'    => $this->newFile,
            'overwrite' => true]);

        copy($this->oldFile, $this->newFile);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newFile,
                'overwrite' => true,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter->getNewName($this->oldFile));
    }

    /**
     * @return void
     */
    public function testGetRandomizedFile()
    {
        $filter = new FileRename([
            'source'    => $this->oldFile,
            'target'    => $this->newFile,
            'randomize' => true
        ]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => true,
            ]],
            $filter->getFile()
        );
        $fileNoExt = $this->tmpPath . DIRECTORY_SEPARATOR . 'newfile';
        $this->assertRegExp('#' . preg_quote($fileNoExt) . '_.{13}\.xml#', $filter->getNewName($this->oldFile));
    }

    /**
     * @return void
     */
    public function testGetRandomizedFileWithoutExtension()
    {
        $fileNoExt = $this->tmpPath . DIRECTORY_SEPARATOR . 'newfile';
        $filter = new FileRename([
            'source'    => $this->oldFile,
            'target'    => $fileNoExt,
            'randomize' => true
        ]);

        $this->assertEquals(
            [0 => [
                'source'    => $this->oldFile,
                'target'    => $fileNoExt,
                'overwrite' => false,
                'randomize' => true,
            ]],
            $filter->getFile()
        );
        $this->assertRegExp('#' . preg_quote($fileNoExt) . '_.{13}#', $filter->getNewName($this->oldFile));
    }

    /**
     * @return void
     */
    public function testAddFileWithString()
    {
        $filter = new FileRename($this->oldFile);
        $filter->addFile($this->newFile);

        $this->assertEquals(
            [0 => [
                'source'    => '*',
                'target'    => $this->newFile,
                'overwrite' => false,
                'randomize' => false,
            ]],
            $filter->getFile()
        );
        $this->assertEquals($this->newFile, $filter($this->oldFile));
        $this->assertEquals('falsefile', $filter('falsefile'));
    }

    /**
     * @return void
     */
    public function testAddFileWithInvalidOption()
    {
        $filter = new FileRename($this->oldFile);
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid options');
        $filter->addFile(1234);
    }

    /**
     * @return void
     */
    public function testInvalidConstruction()
    {
        $this->expectException(Exception\InvalidArgumentException::class);
        $this->expectExceptionMessage('Invalid options');
        $filter = new FileRename(1234);
    }

    public function returnUnfilteredDataProvider()
    {
        return [
            [null],
            [new \stdClass()],
            [[
                $this->oldFile,
                $this->origFile
            ]]
        ];
    }

    /**
     * @dataProvider returnUnfilteredDataProvider
     * @return void
     */
    public function testReturnUnfiltered($input)
    {
        $filter = new FileRename($this->newFile);

        $this->assertEquals($input, $filter($input));
    }
}
