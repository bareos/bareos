# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

### Changed
- Fix to bugreport #887

### Removed

## [17.2.4]

### Changed
- Some minor bugfixes

## [17.2.4-rc2]

### Changed
- Fix to bugreport #827
- Localization NL_BE updated
- Miscalculated volume retention fixed
- Minor improvements to run jobs
- Minor adjustments to backup/restore the filebrowser

## [17.2.4-rc1]

### Added
- Job Totals Dashboard Widget
- Running Jobs Dashboard Widget
- Bootstrap Table Extension
- Job submodule run customized jobs
- Job Module: Used Volumes Widget
- New configuration.ini variable (autorefresh_interval) introduced
  to define the dashboard refresh interval (default: 60sec.)
- Slovak localization
- Turkish localization
- Czech localization

### Changed
- Configurations files documentation
- Configurations files restructured
- TLS: Enforce TLSv1_2
- ZF2 components updated to version 2.4.13
- Fix to bugreport #840
- Fix to bugreport #812
- Russian translation improved
- Past24h Jobs Widget

### Removed
- DataTables (jQuery Plugin)
- jqplot (jQuery Plugin)

## [16.2.6] - 2017-06-19

### Changed
- Fix to bugreport #781
- Fix to bugreport #692
- Fix OBS Specfile
- Version update jQuery
- Version update DataTables
- Version update jsTree
- Version update bootstrap-select extension
- Version update Zend Framework 2 components
- Fix to also show verify jobs in the job action tab listing

## [16.2.5] - 2017-02-01

### Added
- Chinese localization
- Spanish localization
- Italian localization

### Changed
- Fix to bugreport #684
- Fix to bugreport #693
- Fix to bugreport #752
- Fix to bugreport #771
- Fix to bugreport #736
- Fix to bugreport #744
- Fix to bugreport #742
- Fix to bugreport #741
- directors.ini tweaks

## [16.2.4] - 2016-10-14

### Added
- Controller Plugin: Required Command ACL validation

### Changed
- Fix OBS Specfile
- Fix Release Media

## [16.2.4-rc1] - 2016-09-23

### Added
- Russian localization
- French localization
- Basic NGINX config
- Enable/Disable Clients
- Enable/Disable Jobs
- Enable/Disable Schedules
- Catalog Handling
- Configurable Tables
- Selenium Test
- Required ZF2 Components now included

### Changed
- Job listings: Also list Consolidate Jobs
- Configuration
- Console Profile
- Documentation
- Packaging
- External Components updated
- Label Media
- Run Jobs Action: Job types extended
- Session Handling

### Removed
- Old PHP Unit Tests removed
- Travis CI hhvm

## [15.2.4] - 2016-06-16

### Added
- Missing Job Levels added

### Changed
- Fix routing
- Session Handling: Cookie lifetime
- Fix Restore Modul Filetree

## [15.2.3] - 2016-02-26

### Changed
- Fix to bugreport #548
- Fix routing
- Packaging

## [15.2.2] - 2015-11-19

### Added
- jQuery plugin library DataTables introduced

### Changed
- Fix to bugreport #534
- API 2 adjustments
- External components update
- Restore modul improvements

## [15.2.1] - 2015-09-18

### Added
- Restore Modul

### Changed
- Documentation
- Configuration
- Packaging
- Session Handling
- Routing fixes

## [14.2.1] - 2015-04-20

### Changed
- External components updated
- Native DIRD connectivity
- Documentation
- Fix rerun jobs
- Fix packaging
- Fix config reader
- Fix to bugreport #434
- Fix to bugreport #429

## [14.2.0] - 2015-02-27

### Added
- Authentication and Session Handling
- PostgreSQL and MySQL compatibility
- Travis CI Tests

### Changed
- jQuery update from version 1.11.1 to version 1.11.2
- Twitter Bootstrap update to version 3.3.1
- Documentation
- Fix packaging
- Fix ZF2 dependencies
- Fix to bugreport #57
- Fix to bugreport #14

