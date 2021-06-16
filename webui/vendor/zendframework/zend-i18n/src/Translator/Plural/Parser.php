<?php
/**
 * @see       https://github.com/zendframework/zend-i18n for the canonical source repository
 * @copyright Copyright (c) 2005-2019 Zend Technologies USA Inc. (https://www.zend.com)
 * @license   https://github.com/zendframework/zend-i18n/blob/master/LICENSE.md New BSD License
 */

namespace Zend\I18n\Translator\Plural;

use Zend\I18n\Exception;

/**
 * Plural rule parser.
 *
 * This plural rule parser is implemented after the article "Top Down Operator
 * Precedence" described in <http://javascript.crockford.com/tdop/tdop.html>.
 */
class Parser
{
    /**
     * String to parse.
     *
     * @var string
     */
    protected $string;

    /**
     * Current lexer position in the string.
     *
     * @var int
     */
    protected $currentPos;

    /**
     * Current token.
     *
     * @var Symbol
     */
    protected $currentToken;

    /**
     * Table of symbols.
     *
     * @var Symbol[]
     */
    protected $symbolTable = [];

    /**
     * Create a new plural parser.
     *
     */
    public function __construct()
    {
        $this->populateSymbolTable();
    }

    /**
     * Populate the symbol table.
     *
     * @return void
     */
    protected function populateSymbolTable()
    {
        // Ternary operators
        $this->registerSymbol('?', 20)->setLeftDenotationGetter(
            // @codingStandardsIgnoreStart Generic.WhiteSpace.ScopeIndent.IncorrectExact
            static function (Symbol $self, Symbol $left) {
                $self->first  = $left;
                $self->second = $self->parser->expression();
                $self->parser->advance(':');
                $self->third  = $self->parser->expression();
                return $self;
            }
            // @codingStandardsIgnoreEnd
        );
        $this->registerSymbol(':');

        // Boolean operators
        $this->registerLeftInfixSymbol('||', 30);
        $this->registerLeftInfixSymbol('&&', 40);

        // Equal operators
        $this->registerLeftInfixSymbol('==', 50);
        $this->registerLeftInfixSymbol('!=', 50);

        // Compare operators
        $this->registerLeftInfixSymbol('>', 50);
        $this->registerLeftInfixSymbol('<', 50);
        $this->registerLeftInfixSymbol('>=', 50);
        $this->registerLeftInfixSymbol('<=', 50);

        // Add operators
        $this->registerLeftInfixSymbol('-', 60);
        $this->registerLeftInfixSymbol('+', 60);

        // Multiply operators
        $this->registerLeftInfixSymbol('*', 70);
        $this->registerLeftInfixSymbol('/', 70);
        $this->registerLeftInfixSymbol('%', 70);

        // Not operator
        $this->registerPrefixSymbol('!', 80);

        // Literals
        $this->registerSymbol('n')->setNullDenotationGetter(
            // @codingStandardsIgnoreStart Generic.WhiteSpace.ScopeIndent.IncorrectExact
            static function (Symbol $self) {
                return $self;
            }
            // @codingStandardsIgnoreEnd
        );
        $this->registerSymbol('number')->setNullDenotationGetter(
            // @codingStandardsIgnoreStart Generic.WhiteSpace.ScopeIndent.IncorrectExact
            static function (Symbol $self) {
                return $self;
            }
            // @codingStandardsIgnoreEnd
        );

        // Parentheses
        $this->registerSymbol('(')->setNullDenotationGetter(
            // @codingStandardsIgnoreStart Generic.WhiteSpace.ScopeIndent.IncorrectExact
            static function (Symbol $self) {
                $expression = $self->parser->expression();
                $self->parser->advance(')');
                return $expression;
            }
            // @codingStandardsIgnoreEnd
        );
        $this->registerSymbol(')');

        // Eof
        $this->registerSymbol('eof');
    }

    /**
     * Register a left infix symbol.
     *
     * @param  string  $id
     * @param  int $leftBindingPower
     * @return void
     */
    protected function registerLeftInfixSymbol($id, $leftBindingPower)
    {
        $this->registerSymbol($id, $leftBindingPower)->setLeftDenotationGetter(
            // @codingStandardsIgnoreStart Generic.WhiteSpace.ScopeIndent.IncorrectExact
            static function (Symbol $self, Symbol $left) use ($leftBindingPower) {
                $self->first  = $left;
                $self->second = $self->parser->expression($leftBindingPower);
                return $self;
            }
            // @codingStandardsIgnoreEnd
        );
    }

    /**
     * Register a right infix symbol.
     *
     * @param  string  $id
     * @param  int $leftBindingPower
     * @return void
     */
    protected function registerRightInfixSymbol($id, $leftBindingPower)
    {
        $this->registerSymbol($id, $leftBindingPower)->setLeftDenotationGetter(
            // @codingStandardsIgnoreStart Generic.WhiteSpace.ScopeIndent.IncorrectExact
            static function (Symbol $self, Symbol $left) use ($leftBindingPower) {
                $self->first  = $left;
                $self->second = $self->parser->expression($leftBindingPower - 1);
                return $self;
            }
            // @codingStandardsIgnoreEnd
        );
    }

    /**
     * Register a prefix symbol.
     *
     * @param  string  $id
     * @param  int $leftBindingPower
     * @return void
     */
    protected function registerPrefixSymbol($id, $leftBindingPower)
    {
        $this->registerSymbol($id, $leftBindingPower)->setNullDenotationGetter(
            // @codingStandardsIgnoreStart Generic.WhiteSpace.ScopeIndent.IncorrectExact
            static function (Symbol $self) use ($leftBindingPower) {
                $self->first  = $self->parser->expression($leftBindingPower);
                $self->second = null;
                return $self;
            }
            // @codingStandardsIgnoreEnd
        );
    }

    /**
     * Register a symbol.
     *
     * @param  string  $id
     * @param  int $leftBindingPower
     * @return Symbol
     */
    protected function registerSymbol($id, $leftBindingPower = 0)
    {
        if (isset($this->symbolTable[$id])) {
            $symbol = $this->symbolTable[$id];
            $symbol->leftBindingPower = max(
                $symbol->leftBindingPower,
                $leftBindingPower
            );
        } else {
            $symbol = new Symbol($this, $id, $leftBindingPower);
            $this->symbolTable[$id] = $symbol;
        }

        return $symbol;
    }

    /**
     * Get a new symbol.
     *
     * @param string $id
     * @return Symbol
     */
    protected function getSymbol($id)
    {
        if (! isset($this->symbolTable[$id])) {
            // Unknown symbol exception
        }

        return clone $this->symbolTable[$id];
    }

    /**
     * Parse a string.
     *
     * @param  string $string
     * @return Symbol
     */
    public function parse($string)
    {
        $this->string       = $string . "\0";
        $this->currentPos   = 0;
        $this->currentToken = $this->getNextToken();

        return $this->expression();
    }

    /**
     * Parse an expression.
     *
     * @param  int $rightBindingPower
     * @return Symbol
     */
    public function expression($rightBindingPower = 0)
    {
        $token              = $this->currentToken;
        $this->currentToken = $this->getNextToken();
        $left               = $token->getNullDenotation();

        while ($rightBindingPower < $this->currentToken->leftBindingPower) {
            $token              = $this->currentToken;
            $this->currentToken = $this->getNextToken();
            $left               = $token->getLeftDenotation($left);
        }

        return $left;
    }

    /**
     * Advance the current token and optionally check the old token id.
     *
     * @param  string $id
     * @return void
     * @throws Exception\ParseException
     */
    public function advance($id = null)
    {
        if ($id !== null && $this->currentToken->id !== $id) {
            throw new Exception\ParseException(
                sprintf('Expected token with id %s but received %s', $id, $this->currentToken->id)
            );
        }

        $this->currentToken = $this->getNextToken();
    }

    /**
     * Get the next token.
     *
     * @return Symbol
     * @throws Exception\ParseException
     */
    protected function getNextToken()
    {
        while ($this->string[$this->currentPos] === ' ' || $this->string[$this->currentPos] === "\t") {
            $this->currentPos++;
        }

        $result = $this->string[$this->currentPos++];
        $value  = null;

        switch ($result) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                while (ctype_digit($this->string[$this->currentPos])) {
                    $result .= $this->string[$this->currentPos++];
                }

                $id    = 'number';
                $value = (int) $result;
                break;

            case '=':
            case '&':
            case '|':
                if ($this->string[$this->currentPos] === $result) {
                    $this->currentPos++;
                    $id = $result . $result;
                } else {
                    // Yield error
                }
                break;

            case '!':
            case '<':
            case '>':
                if ($this->string[$this->currentPos] === '=') {
                    $this->currentPos++;
                    $result .= '=';
                }

                $id = $result;
                break;

            case '*':
            case '/':
            case '%':
            case '+':
            case '-':
            case 'n':
            case '?':
            case ':':
            case '(':
            case ')':
                $id = $result;
                break;

            case ';':
            case "\n":
            case "\0":
                $id = 'eof';
                $this->currentPos--;
                break;

            default:
                throw new Exception\ParseException(sprintf(
                    'Found invalid character "%s" in input stream',
                    $result
                ));
        }

        $token = $this->getSymbol($id);
        $token->value = $value;

        return $token;
    }
}
