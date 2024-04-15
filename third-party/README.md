# Bareos third party libraries

This is the "home" of external libraries that are used in Bareos

### Adding third party libraries

Each Library will be put into its own subdirectory with `git subtree` and will have its own CMake include-file (if it is not already made available by the provider of the library), that is then loaded in CMakeLists.txt

A typical git subtree command would look like this : `git subtree add --prefix destination/path https://www.yourgithost.com/your/repo yourbranch(usuallymaster) --squash`

Make sure the library license is covert by `LICENSE.txt`. At best, the library is also licensed as AGPL-3. If it is not already covered, a section needs to be added for it in `LICENSE.template`.
Also execute `devtools/update-license-file.sh` to update `LICENSE.txt`, and commit new version.

### What to add?

External things that we rely on (but don't want to change) should live here. Things like `FMT` and `GSL` could be added here. 
`lmdb` (and maybe the `md5` code) should be migrated here.

Things like `droplet`, `fastlz` and `ndmp` will stay as is, because there is no upstream and we made significant changes (i.e. we imported that code into our code-base instead of just using it).
