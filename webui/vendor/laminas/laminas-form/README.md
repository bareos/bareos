# laminas-form

[![Build Status](https://travis-ci.org/laminas/laminas-form.svg?branch=master)](https://travis-ci.org/laminas/laminas-form)
[![Coverage Status](https://coveralls.io/repos/laminas/laminas-form/badge.svg?branch=master)](https://coveralls.io/r/laminas/laminas-form?branch=master)

The `Laminas\Form` is intended primarily as a bridge between your domain models and
the View Layer. It composes a thin layer of objects representing form elements,
an InputFilter, and a small number of methods for binding data to and from the
form and attached objects.

CVE-2022-23598 states that versions earlier than 2.7.14 are vulnerable, but
that is not correct for at least version 2.6.0.

The problem in the CVE lies in missing HTML escaping when displaying
error messages.

However, the function FormElementErrors::render() does HTML escaping in 2.6.0.
This gets removed in version 2.12.0 and is added again in 2.14.2 as
a bugfix for the CVE.

- File issues at https://github.com/laminas/laminas-form/issues
- Documentation is at https://docs.laminas.dev/laminas-form
