# Setup
Clone this repository:
`git clone https://github.gatech.edu/cs6210-aos/project4-oncampus.git`

# Dependencies 
**This is same as project 3. So you can skip the rest of this page, if you have the same system on which you developed the previous project**
  1. CMake/vcpkg based setup -> [setup.md](https://github.gatech.edu/cs6210-aos/project3-oncampus/blob/master/setup.md)

# Keeping your code upto date
Although you are going to submit your solution through t-square only, after the fork followed by clone, we recommend creating a branch and developing your code on that branch:

`git checkout -b develop`

(assuming develop is the name of your branch)

Should the TAs need to push out an update to the assignment, commit (or stash if you are more comfortable with git) the changes that are unsaved in your repository:

`git commit -am "<some funny message>"`

Then update the master branch from remote:

`git pull origin master`

This updates your local copy of the master branch. Now try to merge the master branch into your development branch:

`git merge master`

(assuming that you are on your development branch)

There are likely to be merge conflicts during this step. If so, first check what files are in conflict:

`git status`

The files in conflict are the ones that are "Not staged for commit". Open these files using your favourite editor and look for lines containing `<<<<` and `>>>>`. Resolve conflicts as seems best (ask a TA if you are confused!) and then save the file. Once you have resolved all conflicts, stage the files that were in conflict:

`git add -A .`

Finally, commit the new updates to your branch and continue developing:

`git commit -am "<I did it>"`
