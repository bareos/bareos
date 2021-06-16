<?php
/**
 * Zend Framework (http://framework.zend.com/)
 *
 * @link      http://github.com/zendframework/zf2 for the canonical source repository
 * @copyright Copyright (c) 2005-2015 Zend Technologies USA Inc. (http://www.zend.com)
 * @license   http://framework.zend.com/license/new-bsd New BSD License
 */

namespace ZendTest\View\Helper\Navigation;

use Zend\Config;
use Zend\Navigation\Page\AbstractPage;
use Zend\Navigation\Page\Uri as UriPage;
use Zend\Permissions\Acl;
use Zend\Permissions\Acl\Role;
use Zend\Permissions\Acl\Resource;
use Zend\View;
use Zend\View\Helper\Navigation;

/**
 * Tests Zend\View\Helper\Navigation\Links
 *
 * @group      Zend_View
 * @group      Zend_View_Helper
 */
class LinksTest extends AbstractTest
{
    // @codingStandardsIgnoreStart
    /**
     * Class name for view helper to test
     *
     * @var string
     */
    protected $_helperName = Navigation\Links::class;

    /**
     * View helper
     *
     * @var Navigation\Links
     */
    protected $_helper;

    private $_doctypeHelper;
    private $_oldDoctype;
    // @codingStandardsIgnoreEnd

    public function setUp()
    {
        parent::setUp();

        // doctype fix (someone forgot to clean up after their unit tests)
        $this->_doctypeHelper = $this->_helper->getView()->plugin('doctype');
        $this->_oldDoctype = $this->_doctypeHelper->getDoctype();
        $this->_doctypeHelper->setDoctype(
            \Zend\View\Helper\Doctype::HTML4_LOOSE
        );

        // disable all active pages
        foreach ($this->_helper->findAllByActive(true) as $page) {
            $page->active = false;
        }
    }

    public function tearDown()
    {
        return;
        $this->_doctypeHelper->setDoctype($this->_oldDoctype);
    }

    public function testCanRenderFromServiceAlias()
    {
        $sm = $this->serviceManager;
        $this->_helper->setServiceLocator($sm);

        $returned = $this->_helper->render('Navigation');
        $this->assertEquals($returned, $this->_getExpected('links/default.html'));
    }

    public function testHelperEntryPointWithoutAnyParams()
    {
        $returned = $this->_helper->__invoke();
        $this->assertEquals($this->_helper, $returned);
        $this->assertEquals($this->_nav1, $returned->getContainer());
    }

    public function testHelperEntryPointWithContainerParam()
    {
        $returned = $this->_helper->__invoke($this->_nav2);
        $this->assertEquals($this->_helper, $returned);
        $this->assertEquals($this->_nav2, $returned->getContainer());
    }

    public function testDoNotRenderIfNoPageIsActive()
    {
        $this->assertEquals('', $this->_helper->render());
    }

    public function testDetectRelationFromStringPropertyOfActivePage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $active->addRel('example', 'http://www.example.com/');
        $found = $this->_helper->findRelation($active, 'rel', 'example');

        $expected = [
            'type'  => UriPage::class,
            'href'  => 'http://www.example.com/',
            'label' => null
        ];

        $actual = [
            'type'  => get_class($found),
            'href'  => $found->getHref(),
            'label' => $found->getLabel()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testDetectRelationFromPageInstancePropertyOfActivePage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $active->addRel('example', AbstractPage::factory([
            'uri' => 'http://www.example.com/',
            'label' => 'An example page'
        ]));
        $found = $this->_helper->findRelExample($active);

        $expected = [
            'type'  => UriPage::class,
            'href'  => 'http://www.example.com/',
            'label' => 'An example page'
        ];

        $actual = [
            'type'  => get_class($found),
            'href'  => $found->getHref(),
            'label' => $found->getLabel()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testDetectRelationFromArrayPropertyOfActivePage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $active->addRel('example', [
            'uri' => 'http://www.example.com/',
            'label' => 'An example page'
        ]);
        $found = $this->_helper->findRelExample($active);

        $expected = [
            'type'  => UriPage::class,
            'href'  => 'http://www.example.com/',
            'label' => 'An example page'
        ];

        $actual = [
            'type'  => get_class($found),
            'href'  => $found->getHref(),
            'label' => $found->getLabel()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testDetectRelationFromConfigInstancePropertyOfActivePage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $active->addRel('example', new Config\Config([
            'uri' => 'http://www.example.com/',
            'label' => 'An example page'
        ]));
        $found = $this->_helper->findRelExample($active);

        $expected = [
            'type'  => UriPage::class,
            'href'  => 'http://www.example.com/',
            'label' => 'An example page'
        ];

        $actual = [
            'type'  => get_class($found),
            'href'  => $found->getHref(),
            'label' => $found->getLabel()
        ];

        $this->assertEquals($expected, $actual);
    }

    public function testDetectMultipleRelationsFromArrayPropertyOfActivePage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');

        $active->addRel('alternate', [
            [
                'label' => 'foo',
                'uri'   => 'bar'
            ],
            [
                'label' => 'baz',
                'uri'   => 'bat'
            ]
        ]);

        $found = $this->_helper->findRelAlternate($active);

        $expected = ['type' => 'array', 'count' => 2];
        $actual = ['type' => gettype($found), 'count' => count($found)];
        $this->assertEquals($expected, $actual);
    }

    public function testDetectMultipleRelationsFromConfigPropertyOfActivePage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');

        $active->addRel('alternate', new Config\Config([
            [
                'label' => 'foo',
                'uri'   => 'bar'
            ],
            [
                'label' => 'baz',
                'uri'   => 'bat'
            ]
        ]));

        $found = $this->_helper->findRelAlternate($active);

        $expected = ['type' => 'array', 'count' => 2];
        $actual = ['type' => gettype($found), 'count' => count($found)];
        $this->assertEquals($expected, $actual);
    }

    public function testExtractingRelationsFromPageProperties()
    {
        $types = [
            'alternate', 'stylesheet', 'start', 'next', 'prev', 'contents',
            'index', 'glossary', 'copyright', 'chapter', 'section', 'subsection',
            'appendix', 'help', 'bookmark'
        ];

        $samplePage = AbstractPage::factory([
            'label' => 'An example page',
            'uri'   => 'http://www.example.com/'
        ]);

        $active = $this->_helper->findOneByLabel('Page 2');
        $expected = [];
        $actual = [];

        foreach ($types as $type) {
            $active->addRel($type, $samplePage);
            $found = $this->_helper->findRelation($active, 'rel', $type);

            $expected[$type] = $samplePage->getLabel();
            $actual[$type]   = $found->getLabel();

            $active->removeRel($type);
        }

        $this->assertEquals($expected, $actual);
    }

    public function testFindStartPageByTraversal()
    {
        $active = $this->_helper->findOneByLabel('Page 2.1');
        $expected = 'Home';
        $actual = $this->_helper->findRelStart($active)->getLabel();
        $this->assertEquals($expected, $actual);
    }

    public function testDoNotFindStartWhenGivenPageIsTheFirstPage()
    {
        $active = $this->_helper->findOneByLabel('Home');
        $actual = $this->_helper->findRelStart($active);
        $this->assertNull($actual, 'Should not find any start page');
    }

    public function testFindNextPageByTraversalShouldFindChildPage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $expected = 'Page 2.1';
        $actual = $this->_helper->findRelNext($active)->getLabel();
        $this->assertEquals($expected, $actual);
    }

    public function testFindNextPageByTraversalShouldFindSiblingPage()
    {
        $active = $this->_helper->findOneByLabel('Page 2.1');
        $expected = 'Page 2.2';
        $actual = $this->_helper->findRelNext($active)->getLabel();
        $this->assertEquals($expected, $actual);
    }

    public function testFindNextPageByTraversalShouldWrap()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2.2');
        $expected = 'Page 2.3';
        $actual = $this->_helper->findRelNext($active)->getLabel();
        $this->assertEquals($expected, $actual);
    }

    public function testFindPrevPageByTraversalShouldFindParentPage()
    {
        $active = $this->_helper->findOneByLabel('Page 2.1');
        $expected = 'Page 2';
        $actual = $this->_helper->findRelPrev($active)->getLabel();
        $this->assertEquals($expected, $actual);
    }

    public function testFindPrevPageByTraversalShouldFindSiblingPage()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2');
        $expected = 'Page 2.1';
        $actual = $this->_helper->findRelPrev($active)->getLabel();
        $this->assertEquals($expected, $actual);
    }

    public function testFindPrevPageByTraversalShouldWrap()
    {
        $active = $this->_helper->findOneByLabel('Page 2.3');
        $expected = 'Page 2.2.2';
        $actual = $this->_helper->findRelPrev($active)->getLabel();
        $this->assertEquals($expected, $actual);
    }

    public function testShouldFindChaptersFromFirstLevelOfPagesInContainer()
    {
        $active = $this->_helper->findOneByLabel('Page 2.3');
        $found = $this->_helper->findRelChapter($active);

        $expected = ['Page 1', 'Page 2', 'Page 3', 'Zym'];
        $actual = [];
        foreach ($found as $page) {
            $actual[] = $page->getLabel();
        }

        $this->assertEquals($expected, $actual);
    }

    public function testFindingChaptersShouldExcludeSelfIfChapter()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $found = $this->_helper->findRelChapter($active);

        $expected = ['Page 1', 'Page 3', 'Zym'];
        $actual = [];
        foreach ($found as $page) {
            $actual[] = $page->getLabel();
        }

        $this->assertEquals($expected, $actual);
    }

    public function testFindSectionsWhenActiveChapterPage()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $found = $this->_helper->findRelSection($active);
        $expected = ['Page 2.1', 'Page 2.2', 'Page 2.3'];
        $actual = [];
        foreach ($found as $page) {
            $actual[] = $page->getLabel();
        }
        $this->assertEquals($expected, $actual);
    }

    public function testDoNotFindSectionsWhenActivePageIsASection()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2');
        $found = $this->_helper->findRelSection($active);
        $this->assertNull($found);
    }

    public function testDoNotFindSectionsWhenActivePageIsASubsection()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2.1');
        $found = $this->_helper->findRelation($active, 'rel', 'section');
        $this->assertNull($found);
    }

    public function testFindSubsectionWhenActivePageIsSection()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2');
        $found = $this->_helper->findRelSubsection($active);

        $expected = ['Page 2.2.1', 'Page 2.2.2'];
        $actual = [];
        foreach ($found as $page) {
            $actual[] = $page->getLabel();
        }
        $this->assertEquals($expected, $actual);
    }

    public function testDoNotFindSubsectionsWhenActivePageIsASubSubsection()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2.1');
        $found = $this->_helper->findRelSubsection($active);
        $this->assertNull($found);
    }

    public function testDoNotFindSubsectionsWhenActivePageIsAChapter()
    {
        $active = $this->_helper->findOneByLabel('Page 2');
        $found = $this->_helper->findRelSubsection($active);
        $this->assertNull($found);
    }

    public function testFindRevSectionWhenPageIsSection()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2');
        $found = $this->_helper->findRevSection($active);
        $this->assertEquals('Page 2', $found->getLabel());
    }

    public function testFindRevSubsectionWhenPageIsSubsection()
    {
        $active = $this->_helper->findOneByLabel('Page 2.2.1');
        $found = $this->_helper->findRevSubsection($active);
        $this->assertEquals('Page 2.2', $found->getLabel());
    }

    public function testAclFiltersAwayPagesFromPageProperty()
    {
        $acl = new Acl\Acl();
        $acl->addRole(new Role\GenericRole('member'));
        $acl->addRole(new Role\GenericRole('admin'));
        $acl->addResource(new Resource\GenericResource('protected'));
        $acl->allow('admin', 'protected');
        $this->_helper->setAcl($acl);
        $this->_helper->setRole($acl->getRole('member'));

        $samplePage = AbstractPage::factory([
            'label'    => 'An example page',
            'uri'      => 'http://www.example.com/',
            'resource' => 'protected'
        ]);

        $active = $this->_helper->findOneByLabel('Home');
        $expected = [
            'alternate'  => false,
            'stylesheet' => false,
            'start'      => false,
            'next'       => 'Page 1',
            'prev'       => false,
            'contents'   => false,
            'index'      => false,
            'glossary'   => false,
            'copyright'  => false,
            'chapter'    => 'array(4)',
            'section'    => false,
            'subsection' => false,
            'appendix'   => false,
            'help'       => false,
            'bookmark'   => false
        ];
        $actual = [];

        foreach ($expected as $type => $discard) {
            $active->addRel($type, $samplePage);

            $found = $this->_helper->findRelation($active, 'rel', $type);
            if (null === $found) {
                $actual[$type] = false;
            } elseif (is_array($found)) {
                $actual[$type] = 'array(' . count($found) . ')';
            } else {
                $actual[$type] = $found->getLabel();
            }
        }

        $this->assertEquals($expected, $actual);
    }

    public function testAclFiltersAwayPagesFromContainerSearch()
    {
        $acl = new Acl\Acl();
        $acl->addRole(new Role\GenericRole('member'));
        $acl->addRole(new Role\GenericRole('admin'));
        $acl->addResource(new Resource\GenericResource('protected'));
        $acl->allow('admin', 'protected');
        $this->_helper->setAcl($acl);
        $this->_helper->setRole($acl->getRole('member'));

        $oldContainer = $this->_helper->getContainer();
        $container = $this->_helper->getContainer();
        $iterator = new \RecursiveIteratorIterator(
            $container,
            \RecursiveIteratorIterator::SELF_FIRST
        );
        foreach ($iterator as $page) {
            $page->resource = 'protected';
        }
        $this->_helper->setContainer($container);

        $active = $this->_helper->findOneByLabel('Home');
        $search = [
            'start'      => 'Page 1',
            'next'       => 'Page 1',
            'prev'       => 'Page 1.1',
            'chapter'    => 'Home',
            'section'    => 'Page 1',
            'subsection' => 'Page 2.2'
        ];

        $expected = [];
        $actual = [];

        foreach ($search as $type => $active) {
            $expected[$type] = false;

            $active = $this->_helper->findOneByLabel($active);
            $found = $this->_helper->findRelation($active, 'rel', $type);

            if (null === $found) {
                $actual[$type] = false;
            } elseif (is_array($found)) {
                $actual[$type] = 'array(' . count($found) . ')';
            } else {
                $actual[$type] = $found->getLabel();
            }
        }

        $this->assertEquals($expected, $actual);
    }

    public function testFindRelationMustSpecifyRelOrRev()
    {
        $active = $this->_helper->findOneByLabel('Home');
        try {
            $this->_helper->findRelation($active, 'foo', 'bar');
            $this->fail('An invalid value was given, but a ' .
                        'Zend\View\Exception\InvalidArgumentException was not thrown');
        } catch (View\Exception\ExceptionInterface $e) {
            $this->assertContains('Invalid argument: $rel', $e->getMessage());
        }
    }

    public function testRenderLinkMustSpecifyRelOrRev()
    {
        $active = $this->_helper->findOneByLabel('Home');
        try {
            $this->_helper->renderLink($active, 'foo', 'bar');
            $this->fail('An invalid value was given, but a ' .
                        'Zend\View\Exception\InvalidArgumentException was not thrown');
        } catch (View\Exception\ExceptionInterface $e) {
            $this->assertContains('Invalid relation attribute', $e->getMessage());
        }
    }

    public function testFindAllRelations()
    {
        $expectedRelations = [
            'alternate'  => ['Forced page'],
            'stylesheet' => ['Forced page'],
            'start'      => ['Forced page'],
            'next'       => ['Forced page'],
            'prev'       => ['Forced page'],
            'contents'   => ['Forced page'],
            'index'      => ['Forced page'],
            'glossary'   => ['Forced page'],
            'copyright'  => ['Forced page'],
            'chapter'    => ['Forced page'],
            'section'    => ['Forced page'],
            'subsection' => ['Forced page'],
            'appendix'   => ['Forced page'],
            'help'       => ['Forced page'],
            'bookmark'   => ['Forced page'],
            'canonical'  => ['Forced page'],
            'home'       => ['Forced page']
        ];

        // build expected result
        $expected = [
            'rel' => $expectedRelations,
            'rev' => $expectedRelations
        ];

        // find active page and create page to use for relations
        $active = $this->_helper->findOneByLabel('Page 1');
        $forcedRelation = new UriPage([
            'label' => 'Forced page',
            'uri'   => '#'
        ]);

        // add relations to active page
        foreach ($expectedRelations as $type => $discard) {
            $active->addRel($type, $forcedRelation);
            $active->addRev($type, $forcedRelation);
        }

        // build actual result
        $actual = $this->_helper->findAllRelations($active);
        foreach ($actual as $attrib => $relations) {
            foreach ($relations as $type => $pages) {
                foreach ($pages as $key => $page) {
                    $actual[$attrib][$type][$key] = $page->getLabel();
                }
            }
        }

        $this->assertEquals($expected, $actual);
    }

    // @codingStandardsIgnoreStart
    private function _getFlags()
    {
        // @codingStandardsIgnoreEnd
        return [
            Navigation\Links::RENDER_ALTERNATE  => 'alternate',
            Navigation\Links::RENDER_STYLESHEET => 'stylesheet',
            Navigation\Links::RENDER_START      => 'start',
            Navigation\Links::RENDER_NEXT       => 'next',
            Navigation\Links::RENDER_PREV       => 'prev',
            Navigation\Links::RENDER_CONTENTS   => 'contents',
            Navigation\Links::RENDER_INDEX      => 'index',
            Navigation\Links::RENDER_GLOSSARY   => 'glossary',
            Navigation\Links::RENDER_CHAPTER    => 'chapter',
            Navigation\Links::RENDER_SECTION    => 'section',
            Navigation\Links::RENDER_SUBSECTION => 'subsection',
            Navigation\Links::RENDER_APPENDIX   => 'appendix',
            Navigation\Links::RENDER_HELP       => 'help',
            Navigation\Links::RENDER_BOOKMARK   => 'bookmark',
            Navigation\Links::RENDER_CUSTOM     => 'canonical'
        ];
    }

    public function testSingleRenderFlags()
    {
        $active = $this->_helper->findOneByLabel('Home');
        $active->active = true;

        $expected = [];
        $actual   = [];

        // build expected and actual result
        foreach ($this->_getFlags() as $newFlag => $type) {
            // add forced relation
            $active->addRel($type, 'http://www.example.com/');
            $active->addRev($type, 'http://www.example.com/');

            $this->_helper->setRenderFlag($newFlag);
            $expectedOutput = '<link '
                              . 'rel="' . $type . '" '
                              . 'href="http&#x3A;&#x2F;&#x2F;www.example.com&#x2F;">' . PHP_EOL
                            . '<link '
                              . 'rev="' . $type . '" '
                              . 'href="http&#x3A;&#x2F;&#x2F;www.example.com&#x2F;">';
            $actualOutput = $this->_helper->render();

            $expected[$type] = $expectedOutput;
            $actual[$type]   = $actualOutput;

            // remove forced relation
            $active->removeRel($type);
            $active->removeRev($type);
        }

        $this->assertEquals($expected, $actual);
    }

    public function testRenderFlagBitwiseOr()
    {
        $newFlag = Navigation\Links::RENDER_NEXT |
                   Navigation\Links::RENDER_PREV;
        $this->_helper->setRenderFlag($newFlag);
        $active = $this->_helper->findOneByLabel('Page 1.1');
        $active->active = true;

        // test data
        $expected = '<link rel="next" href="page2" title="Page&#x20;2">'
                  . PHP_EOL
                  . '<link rel="prev" href="page1" title="Page&#x20;1">';
        $actual = $this->_helper->render();

        $this->assertEquals($expected, $actual);
    }

    public function testIndenting()
    {
        $active = $this->_helper->findOneByLabel('Page 1.1');
        $newFlag = Navigation\Links::RENDER_NEXT |
                   Navigation\Links::RENDER_PREV;
        $this->_helper->setRenderFlag($newFlag);
        $this->_helper->setIndent('  ');
        $active->active = true;

        // build expected and actual result
        $expected = '  <link rel="next" href="page2" title="Page&#x20;2">'
                  . PHP_EOL
                  . '  <link rel="prev" href="page1" title="Page&#x20;1">';
        $actual = $this->_helper->render();

        $this->assertEquals($expected, $actual);
    }

    public function testSetMaxDepth()
    {
        $this->_helper->setMaxDepth(1);
        $this->_helper->findOneByLabel('Page 2.3.3')->setActive(); // level 2
        $flag = Navigation\Links::RENDER_NEXT;

        $expected = '<link rel="next" href="page2&#x2F;page2_3&#x2F;page2_3_1" title="Page&#x20;2.3.1">';
        $actual = $this->_helper->setRenderFlag($flag)->render();

        $this->assertEquals($expected, $actual);
    }

    public function testSetMinDepth()
    {
        $this->_helper->setMinDepth(2);
        $this->_helper->findOneByLabel('Page 2.3')->setActive(); // level 1
        $flag = Navigation\Links::RENDER_NEXT;

        $expected = '';
        $actual = $this->_helper->setRenderFlag($flag)->render();

        $this->assertEquals($expected, $actual);
    }

    /**
     * Returns the contens of the expected $file, normalizes newlines
     * @param  string $file
     * @return string
     */
    // @codingStandardsIgnoreStart
    protected function _getExpected($file)
    {
        // @codingStandardsIgnoreEnd
        return str_replace("\n", PHP_EOL, parent::_getExpected($file));
    }
}
