# USING THE GIT REPOSITORY

## Setup your own public repository

Your first step is to establish a public repository from which we can
pull your work into the master repository. 

While you can use a private repository and utilize ``git format-patch`` to
submit patches, this is discouraged as it does not facilitate public peer
review. So please contribute by pull request.

1. Setup a GitHub account (http://github.com/), if you haven't yet.
2. Fork the Barbossa repository (http://github.com/fbergkemper/barbossa)
3. Clone your fork locally and enter it (use your own GitHub username in the statement below)

```
sh% git clone git@github.com:<username>/barbossa.git
% cd barbossa
```

4. Add a remote to the canonical Barbossa repository, so you can keep your fork up-to-date:

```
sh% git remote add upstream https://github.com/fbergkemper/barbossa.git
sh% git fetch upstream
```

## Everyday workflow

When working on Barbossa, we recommend you do each new feature or bugfix in a new branch. 
This simplifies the task of code review as well as of merging your changes into the canonical 
repository. A typical work flow will then consist of the following:

1. Create a new local branch based off your master branch.
2. Switch to your new local branch.
3. Do some work, commit, repeat as necessary.
4. Push the local branch to your remote repository.
5. Send a pull request.

