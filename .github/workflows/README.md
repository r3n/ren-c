# GitHub Actions

Bash Style Ideas:

https://ae1020.github.io/thoughts-on-bash-style/

Links for future investigation:

https://github.com/fkirc/skip-duplicate-actions

## Action Security

**IMPORTANT**: GitHub Actions has a very convenient feature with the `uses:`
clause, that lets you reuse script code that others have posted on GitHub
to do convenient setup work.  But this running of arbitrary code can have a
lot of failure modes.

* If you provide credentials to the container, those credentials can be abused
  by a malicious action.  If you make it possible for the container to upload
  or delete files from an AWS bucket, then the action can do that too...
  possibly even stealing the credentials to use elsewhere as well.

* When the purpose of a container is to make build products to be pushed out
  elsewhere, the action could tamper with the toolchains in order to build
  tainted products.  The compiler could be hooked with a script that adds a
  virus payload onto executables, etc.

* The most likely form of problem is simply that the repository containing the
  action could vanish at some inopportune time.

Of course, there's no limit to the amount of paranoia one could potentally
have regarding dependencies.  (This is one of the reasons Ren-C is so strict
about dependency control in the compilation process in the first place!)

For the purposes of this project, we will assume it is sufficiently secure to
use actions from trusted GitHub accounts (e.g. GitHub itself, Microsoft, or
Amazon).  Any actions under "random" user accounts should be at minimum
referenced by specific hash instead of a branch name or tag label...and the
code should be given at least a cursory inspection each time that commit is
updated.

YCombinator thread: https://news.ycombinator.com/item?id=21844805
