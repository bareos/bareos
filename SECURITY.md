# Security Policy

## Reporting a security issue

If you find a security-related problem in Bareos, please report it via e-mail to
security@bareos.org.
We will try to respond to you within two days.
Please be patient with us.

Together with you, we will then determine a CVSS score and find out which
versions of Bareos are vulnerable.
Based on that information we will decide what actions we will take.

## Supported Versions

Besides the `master`-branch of the project in the `next` repository, we maintain the
three newest major releases.
Binary packages of maintenance releases for the major-branches are only shipped
to customers with a subscription.
However, binary packages of the latest pre-release - which will also contain
every released fix - will be available to the general public.

| Repository                                           | Description           | Maintained         |
| ----------                                           | -----------           | ------------------ |
| [25](https://download.bareos.com/bareos/release/25/) | Bareos 25 releases    | :heavy_check_mark: |
| [24](https://download.bareos.com/bareos/release/24/) | Bareos 24 releases    | :heavy_check_mark: |
| [23](https://download.bareos.com/bareos/release/23/) | Bareos 23 releases    | :heavy_check_mark: |
| <= 22                                                | Older Bareos releases | :x:                | 
| [current](https://download.bareos.org/current/)      | latest pre-release    | :heavy_check_mark: |
| [next](https://download.bareos.org/next/)            | nightly build         | :heavy_check_mark: |

You can find an overview of previously fixed vulnerabilities in the project's
[Security Advisories](https://github.com/bareos/bareos/security/advisories).
