# GitHub Actions

Bash Style Ideas:

https://ae1020.github.io/thoughts-on-bash-style/

Links for future investigation:

https://github.com/fkirc/skip-duplicate-actions

## !!! IMPORTANT - Untrusted Actions, Use Audited Hash !!!

GitHub Actions has a very convenient feature with the `uses:` clause, that lets
you reuse script code that others have posted on GitHub to do convenient setup
work.  But this running of arbitrary code can have a lot of failure modes.

* If you provide credentials to the container, those credentials can be abused
  by a malicious action.  If you make it possible for the container to upload
  or delete files from an AWS bucket, then the action can do that too...
  possibly even stealing the credentials to use elsewhere as well.

* When the purpose of a container is to make build products to be pushed out
  to the world, the action could tamper with the toolchains in order to build
  tainted products.  The compiler could be hooked with a script that adds a
  virus payload onto executables, etc.

* The most likely form of problem is simply that the repository containing the
  action could vanish at some inopportune time.

Of course, there's no limit to the amount of paranoia one could potentally
have regarding dependencies.  (This is one of the reasons Ren-C is so strict
about dependency control in the compilation process in the first place!)

Any actions under "random" user accounts should be at minimum referenced by
specific hash instead of a branch name or tag label...and the code should be
given at least a cursory inspection each time that commit is updated.

YCombinator thread: https://news.ycombinator.com/item?id=21844805

## IMPORTANT - Security Of Trusted Actions

GitHub has verified accounts that host Actions, for entities like Microsoft,
Amazon, or specifically the GitHub service itself.

We'll assume that it's okay to use those actions by tag, without any commit
hash or need for extra review.

# When To Trigger Builds

We try to keep the builds somewhat reined in...to stay on the good side of
GitHub and not exceed any quotas, and to conserve planetary energy (if for no
other reason).

It's set to be possible to trigger any workflow at any time manually (unclear
why anyone would ever *not* enable that feature):

https://github.blog/changelog/2020-07-06-github-actions-manual-triggers-with-workflow_dispatch/

Then, in order to make it easy to debug a particular failing workflow, each one
responds to changes on a named branch that no other one does.  This makes it
easy to push to a branch with that name while working out problems in just
that workflow.

Pull requests to master are fairly rare at this point in time.  So that is used
as a trigger to do a "kitchen sink" build of all the workflows.  That's costly
but infrequent.

Pushes to master do a couple of builds always--such as the web build and the
Windows builds.

Note that we could also use `if` conditions to control this, e.g.

    if: contains(github.event.head_commit.message, 'TCC')

This could be done at the whole job level, or on individual steps.
## Using The Strict Erroring Bash Shell

GitHub Actions fortunately has bash on the Windows Server containers.  While
there might be some theoretical benefit to PowerShell (the default), having
cross-platform code it just makes more sense to keep all the files as bash.

USING BASH ON WINDOWS TAMPERS WITH THE PATH.  This causes strange effects like
there being visibility of GNU's symlink tool /usr/bin/link that is found
instead of the MS compiler's LINK.EXE

https://github.com/ilammy/msvc-dev-cmd/issues/25

For this project, it's considered worth it to work around issues with that
when it comes up...for the sake of consistency across the scripts.

By default, bash does not speak up when an error happens in the middle of
lines of shell code.  So the only error you would get from a long `run` code
for a step would be if the last line had a nonzero exit code (or if something
explicitly called exit(1) at an earlier time).

    $ bash -e -c "cd asdfasdf; echo 'got here despite error'"
    bash: line 0: cd: asdfasdf: No such file or directory
    got here despite error

Fortunately, there's a `-e` feature which overrides this behavior, and stops
on the first error:

    $ bash -e -c "cd asdfasdf; echo 'will not get here'"
    bash: line 0: cd: asdfasdf: No such file or directory

It's much better to use that and use switches to suppress the failing cases
that are supposed to fail.  This is enabled by default when you use the
`shell: bash` option, acts like: `bash --noprofile --norc -eo pipefail {0}`

https://docs.github.com/en/actions/reference/workflow-syntax-for-github-actions#using-a-specific-shell

Be sure to preserve the `-e` setting if customizing the bash command further.


# Ren-C Code As Step

Because GitHub Actions lets you customize the shell, we can actually make the
`run:` portion be Ren-C code.  This lets you cleanly span code across many
lines, and avoids the hassles that come from needing to escape quotes and
apostrophes when you call from bash, e.g. `r3 --do "print \"This Sucks\""`

  step:
    shell: r3 --fragment {0}  # fragment will tolerate CR+LF, won't change dir
    run: |
      print "Hello, Ren-C As Shell World!"
      print ['apostrophes-dont-need-escaping "...and neither do double quotes"]

Specifying the shell as `r3 --do {0}` or `r3 --do "{0}"` doesn't work.  It's
not completely clear why this works...but perhaps it is feeding it from the
standard input device (as if you had typed the script).  Either way, it's
better...because if you had to pass the arguments on the command line you would
be getting problems with the escaping.

The file won't have a header, and on Windows GitHub will add CR LF to the code
even if the original YAML file had plain LF endings.  To support this need
the `--fragment` option was added.  We do not want actual distributed scripts
that make it beyond one individual's use to be carrying CR LF in them, so this
helps tie the tolerances together with this ad hoc nature.

On Windows, it seems the shell path is unconditionally prepended with:

  C:/Program Files/Git/usr/bin/

Most likely because that is the location where the main `shell:` is, and this
is being inherited by `step:`/`shell:`.  Until that is fixed, you have to copy
any shell program you want to use to that location.

## Checkout Action

The checkout action checks-out your repository under $GITHUB_WORKSPACE

https://github.com/actions/checkout

It defaults to checking out at a --depth of 1, which is usually fine.

Note: This strangely starts you up in a directory like:

   /home/runner/work/ren-c/ren-c

The parent directory appears to contain the same files.  It's not clear
why it's done this way...but presumably it's on purpose.

## Jobs

Each "Job" runs in its own VM, and a workflow run is made up of one or more
jobs that can run sequentially or in parallel.

Jobs are isolated from one another (outside of any data they might exchange
indirectly through uploading and downloading from the network).  This means
you can't have a "build job" and a "deploy job" that uploads the build
products of that job, since the deploy wouldn't have access to any of the
files that were built.  "Steps" must be used.

However, you could use separate jobs to build and test (for instance) if the
build job saved its products somewhere the test job could get them.  This can
be on your own servers (e.g. AWS), or GitHub offers its own file services
in the form of what are called "Artifacts":

https://docs.github.com/en/actions/guides/storing-workflow-data-as-artifacts

## Steps

Steps are a sequence of tasks that will be executed within a single VM as part
of the job.

If a step fails, then the default behavior is for the subsequent steps to not
run, though this can be overridden with an `if:` property, which is granular
3nough to allow running only if particular other steps have completed:

https://docs.github.com/en/free-pro-team@latest/actions/reference/context-and-expression-syntax-for-github-actions#job-status-check-functions
https://stackoverflow.com/a/61832535

Each step has its own isolated environment variables, but it is possible to
export state seen by future steps (see `Environment Variables` below).

The exit status of a step is the result of the last executed command in
that step, or the parameter to `exit` if the step ends prematurely.

A step that only contains `uses:` is a means of incorporating a fragment
of GitHub action code published in a separate repository.  Please see notes
above regarding cautions about that.  Also, always link to the repository for
a used action in a comment so that the options (if any) are at hand.

## Environment Variables

Environment variables can be set per step, per job, or for entire workflow
using `env:`.  You can also use calculated variables in the environment, like
from the build matrix:

    env:
      OS_ID: ${{ matrix.os-id }}

But it's transferring variables between steps that gets a
bit tricky.

If you use `export VAR=value` in a step, that will only set the variable within
that particular step.  If you want to transfer variables further than that,
you need to do this:

     echo "VAR=value" >> $GITHUB_ENV

It will be available in the ensuing steps (though not the current one).

https://docs.github.com/en/free-pro-team@latest/actions/reference/environment-variables

Note: If you see files advocating for a syntax like `::set-env::` that is
a deprecated method:

https://github.blog/changelog/2020-10-01-github-actions-deprecating-set-env-and-add-path-commands/

## If Statements

You can control whether a step runs with an if statement.  Important to know
is that this expression is evaluated explicitly inside GitHub's ${{ }}
syntax.  So using the syntax will silently cause undesirable behavor:

    step: Demonstrate Incorrect Use of IF Statement
    if: ${{ matrix.os }} == 'ubuntu-20.04'
    run: echo "This always runs, regardless of what matrix.os is :-("

To avoid this frustration, don't use ${{ }} in the expressions:

    step: Demonstrate Incorrect Use of IF Statement
    if: matrix.os == 'ubuntu-20.04'
    run: echo "This runs only for the specified condition."

For documentation of the list of variables and operators available:

https://docs.github.com/en/actions/reference/context-and-expression-syntax-for-github-actions

## Build Matrix

(Note: We want non-Github-Actions-specific syntax in our steps--e.g. `$FOO`
and not `${{ matrix.foo }}`--so proxy matrix vars to environment vars when
that is feasible.)

The build matrix is where you can either explicitly give a full list of
variables for a build permutation with `include`:

    strategy:
      matrix:
        include:  # this will give you one job where ${{ var1 }} is a
        - var1: a
        - var2: common

        include:  # this will give you antoher job where ${{ var1 }} is b
        - var1: b
        - var2: common

Or you can use [..., ...] syntax to list variables and ask for an automatic
permutation of builds.  So this is a more compact way of saying the above:

    strategy:
      matrix:
        - var1: [a, b]
        - var2: common

The variables set here are GitHub Actions variables, not environment variables.
That means they have to be accessed with ${{ var1 }} syntax.  In order to avoid
tying too much of the process to run only on GitHub actions, it's best to
capture them into environment variables.

https://docs.github.com/en/free-pro-team@latest/actions/reference/workflow-syntax-for-github-actions#using-environment-variables-in-a-matrix

## Portably Capturing Git Hashes

While GitHub Actions may offer this, good to have it done in an agnostic
fashion so the script is more portable.

http://stackoverflow.com/a/42549385


## YAML >- To Make One Line From Many

This operator is useful if you're writing something that is getting too long
to be on one line...but adding newlines to it would break things:

    Key: >-
      this is my very very very
      long string

This produces `this is my very very very long string`.  If you want a newline
at the end, then use `>` instead of `>-`

https://stackoverflow.com/a/21699210
