# USING THE GIT REPOSITORY

## Setup your own public repository

Your first step is to establish a public repository from which we can
pull your work into the master repository. 

While you can use a private repository and utilize ``git format-patch`` to
submit patches, this is discouraged as it does not facilitate public peer
review. So please contribute by pull request.

1. Setup a GitHub account (http://github.com/), if you haven't yet.
2. Fork the bareos-webui repository (http://github.com/bareos/bareos-webui)
3. Clone your fork locally and enter it (use your own GitHub username in the statement below)

```
sh% git clone git@github.com:<username>/bareos-webui.git
% cd bareos-webui
```

4. Add a remote to the canonical bareos-webui repository, so you can keep your fork up-to-date:

```
sh% git remote add upstream https://github.com/bareos/bareos-webui.git
sh% git fetch upstream
```

## Everyday workflow

When working on bareos-webui, we recommend you do each new feature or bugfix in a new branch. 
This simplifies the task of code review as well as of merging your changes into the canonical 
repository. A typical work flow will then consist of the following:

1. Create a new local branch based off your master branch.
2. Switch to your new local branch.
3. Do some work, commit, repeat as necessary.
4. Push the local branch to your remote repository.
5. Send a pull request.

## Keeping Up-to-Date

Periodically, you should update your fork or personal repository to match the canonical bareos-webui 
repository.

```
sh% git checkout master
sh% git pull bareos-webui master
```
To keep your remote up-to-date:

```
sh% git push origin
```

### Branch Cleanup

As you might imagine, if you are a frequent contributor, you'll start to get a ton of branches both 
locally and remote. Once you know that your changes have been accepted to the master repository, we
suggest doing some cleanup of these branches.

Local branch cleanup:

```
sh% git branch -d <branchname>
```

Remote branch removal:

```
sh% git push origin :<branchname>
```

