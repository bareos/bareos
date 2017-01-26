#!/bin/sh

DESTDIR=${1:-"./"}
GIT="git"

which git >/dev/null 2>&1
if test $? != 0; then
    echo "skipped: git not available"
    exit 0
fi

git status >/dev/null 2>&1
if test $? != 0; then
    echo "skipped: this directory isn't a git directory"
    exit 0
fi

LANG=C

git_branch_info()
{
    #git rev-parse --abbrev-ref --symbolic-full-name @{upstream} > git/branch

    LOCAL_BRANCH=`git name-rev --name-only HEAD`
    TRACKING_BRANCH=`git config branch.$LOCAL_BRANCH.merge`
    TRACKING_REMOTE=`git config branch.$LOCAL_BRANCH.remote`
    # remove credentials from url
    REMOTE_URL=`git config remote.$TRACKING_REMOTE.url | sed "s|\(http[s]://\).*@|\1|"`

    printf "remote_url=$REMOTE_URL\n"
    # OBS return always "master" therefore disabled
    # (because OBS uses clone $GITURL + reset --hard $BRANCH)
    #printf "remote_branch=$TRACKING_BRANCH\n"

    RELEASE=`git describe --tags --match "Release/*" --always --abbrev=40`
    printf "release=$RELEASE\n"
}

git_branch_info  > $DESTDIR/git-info

DIFF=`$GIT diff --stat`
[ "$DIFF" ] && printf "$DIFF" > $DESTDIR/git-diff

$GIT log --stat  > $DESTDIR/git-log
