# laminas-i18n

[![Build Status](https://travis-ci.org/laminas/laminas-i18n.svg?branch=master)](https://travis-ci.org/laminas/laminas-i18n)
[![Coverage Status](https://coveralls.io/repos/laminas/laminas-i18n/badge.svg?branch=master)](https://coveralls.io/r/laminas/laminas-i18n?branch=master)

`Laminas\I18n` comes with a complete translation suite which supports all major
formats and includes popular features like plural translations and text domains.
The Translator component is mostly dependency free, except for the fallback to a
default locale, where it relies on the Intl PHP extension.

The translator itself is initialized without any parameters, as any configuration
to it is optional. A translator without any translations will actually do nothing
but just return the given message IDs.

- File issues at https://github.com/laminas/laminas-i18n/issues
- Documentation is at https://docs.laminas.dev/laminas-i18n/
