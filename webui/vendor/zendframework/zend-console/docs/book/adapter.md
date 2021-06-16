# Console adapters

zend-console's console abstraction layer works around various bugs and limitations
in operating systems, including:

- display of colorized text.
- discovery and calculation of console window size.
- discovery of console charset.
- basic line drawing capabilities.

Console adapters implement `Zend\Console\Adapter\AdapterInterface`, and you
should typehint against that interface for purposes of working with console
capabilities. Adapters are included for:

- Posix (\*nix-based systems)
- Windows (and Windows ANSI; for use with `command.bat`)
- Virtual (which provides Windows PowerShell compatibility)

## Retrieving the console adapter

While you may know your current environment, you will want to write your code in
such a way that the console adapter is specific to whatever environment it is
run within. As such, you likely should never instantiate an adapter directly.
zend-console, and the zend-mvc integration, provide tools for retrieving an
appropriate adapter for the current environment in a generic way.

### Standalone

zend-console provides a factory for creating and returning the console adapter
specific to your environment:

```php
use Zend\Console\Console;
use Zend\Console\Exception\ExceptionInterface as ConsoleException;

try {
    $console = Console::getInstance();
} catch (ConsoleException $e) {
    // Could not get console adapter; most likely we are not running inside a
    // console window.
}
```

This returns a `Zend\Console\Adapter\AdapterInterface` implementation suitable
for your current environment.

> #### Exceptions
>
> For practical and security reasons, `Console::getInstance()` will always throw
> an exception if you attempt to get console instance in a non-console
> environment (i.e. when running on a HTTP server). You can override this
> behavior by manually instantiating one of the `Zend\Console\Adapter\*`
> classes; you should do this only as a last resort, however!

### zend-mvc

If you are using MVC controllers you can obtain Console adapter instance using
the `ServiceManager`.

```php
namespace Application;

use Zend\Mvc\Controller\AbstractActionController;
use Zend\Console\Adapter\AdapterInterface as Console;
use Zend\Console\Exception\RuntimeException;

class ConsoleController extends AbstractActionController
{
    public function testAction()
    {
        $console = $this->getServiceLocator()->get('console');
        if (! $console instanceof Console) {
            throw new RuntimeException('Cannot obtain console adapter. Are we running in a
console?');
        }
    }
}
```

If you extend `AbstractConsoleController`, you can use the `getConsole()` method
instead:

```php
namespace Application;

use Zend\Mvc\Controller\AbstractConsoleController;
use Zend\Console\Exception\RuntimeException;

class ConsoleController extends AbstractConsoleController
{
    public function testAction()
    {
        $console = $this->getConsole();
    }
}
```

> #### Use dependency injection
>
> We recommend using dependency injection. Instead of pulling the console
> adapter from the service manager, inject it from within your controller's
> factory. This is essentially what happens when extending
> `AbstractConsoleController` already.

## Using the console adapter

`Zend\Console\Adapter\AdapterInterface` describes a rich set of capabilities.
Below is a sampling.

### Window size and title

Method | Description
------ | -----------
`$console->getWidth()` | (int) Get real console window width in characters.
`$console->getHeight()` | (int) Get real console window height in characters.
`$console->getSize()` | (array) Returns `[$width, $height]` with current console window dimensions.
`$console->getTitle()` | (string) Get console window title.

> #### Multibyte sequences
>
> For UTF-8 enabled consoles, dimensions represent the number of multibyte
> characters (real characters).

> #### Virtual buffer sizes
>
> On consoles with virtual buffers (e.g. MS Windows Command Prompt), width and
> height represent visible (real) size, without scrolling the window. For
> example, if the window scrolling width is 120 chars, but its real, visible
> width is 80 chars, `getWidth()` will return `80`.

### Character set

Method | Description
------ | -----------
`$console->isUtf8()` | (boolean) Is the console UTF-8 compatible (can it display unicode strings)?
`$console->getCharset()` | (`Zend\Console\Charset\CharsetInterface`) This method will return an instance of one of the `Zend\Console\Charset\*` classes representing the readable charset present for line-drawing. It is automatically detected by the adapter.

### Writing to the console

Method | Description
------ | -----------
`$console->write(string $text, $color = null, $bgColor = null)` | Write `$text` to the console, optionally using foreground `$color` and background `$bgColor`.  Color values must be one of the `Zend\Console\ColorInterface` constants.
`$console->writeLine(string $text, $color = null, $bgColor = null)` | Write a single line of `$text` to the console. This method will output an environment-specific newline character at the end of the text, moving the console cursor to next line.
`$console->writeAt(string $text, int $x, int $y, $color = null, $bgColor = null)` | Write `$text` at the specified `$x` and `$y` coordinates of console window. The top left corner of the screen has coordinates of `$x = 1; $y = 1`. To retrieve the far-right and bottom coordinates, use the `getWidth()` and `getHeight()` methods.

### Reading from the console

Method | Description
------ | -----------
`$console->readChar(string $mask = null)` | (string) Read a single character from the console. Optional `(string) $mask` can be provided to force entering only a selected set of characters. For example, to read a single digit, we can use the following syntax: `$digit = $console->readChar('0123456789');`.
`$console->readLine(int $maxLength = 2048)` | (string) Read a single line of input from console. Optional `(int) $maxLength` can be used to limit the length of data that will be read. The line will be returned **without trailing newline characters**.

### Miscellaneous

Method | Description
------ | -----------
`$console->hideCursor()` | Hide blinking cursor from the console.
`$console->showCursor()` | Show blinking cursor in the console.
`$console->clear()` | Clear the screen.
`$console->clearLine()` | Clear the line that the cursor currently sits at.
