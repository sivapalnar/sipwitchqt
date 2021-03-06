Standard Practices
==================

I have chosen several standard practices in those packages I now happen to use **Qt** and **qmake** for, to facilitate consistent cross-platform development, to better support distributed releases, and to provide better testability of my software.  

In standardizing and documenting my own practices, I believe it is important not just for the code to be free, but also how to build a given project and manage the software lifecycle as well.  There is no reason why one cannot produce high quality enterprise ready free as in freedom software, and many reasons why free software can and should produce better results.

## Copyright Assignment

What makes a copyleft license stronger than a generic free software license like the BSD or MIT license is that the freedom it offers to all downstream users cannot be terminated by any intermediary party, and is actively defendable in court.  This requires a copyright holder (or holders) of record with explicit right to do so.  Tycho Softworks is the copyright holder for this package and so we desire all significant contributions to include explicit copyright assignment.

## Build Types

My qmake **release** builds generally are created with debug symbols to facilitate reporting of crash dumps.  Generally they are stripped before shipping binary packages, so I can symbolicated later, such as with a Google handbrake server.  However, I also have a special make target for creating release builds, **archive**, which produces a build/archive that can be used to produce stand-alone builds that can be tested locally or handed out to external testers.  I also have a special make target, **publish**, to produce source archives and release builds for external distribution.

Generally I assume qmake **debug** builds are to be primarily used for local development and testing.  On debug builds I may short-circuit normal application behavior to improve testability, whether executed from qtcreator or stand-alone.  For developing daemon services debug mode uses the project local testdata.  This may include sample configs for running test cases and debug mode built-in unit tests, and offers a simple way to locally test a server configuration during development without requiring administrative access on the development machine itself.

I may produce binary installers out of **release** builds by using an alternative make target.  These alternate targets make it easy to use a release build for local testing (such as using the **publish\_and_install** target in Archive.pri), and to hand off self-contained builds for testing by others.  Actual formal **release** builds are also commonly made with the **publish\_and_archive** or the **publish\_and_install** target to build binaries and release installers.  Source tarballs are produced with each publish target to make it easy to share and redistribute our software in original or modified form using the GNU GPL.

## Code Architecture

In general I use a flat hierarchy where headers and sources co-mingle, and where every source file has a matching header.  This makes it easier for IDE's (like QtCreator) that offer the ability to switch from source to header file.  I also use Qt naming conventions and coding styles for constancy.

Much of the functionality of SipServerQt is divided into individual QOBject components that have and use well defined functionality and use signal-slot interfaces.  This also makes it easy to separate the server into components that can then have their own threads, and helps to avoid larger locking and lock contention with such heavy threading since there are only very narrow locking windows for Qt inter-thread signal-slot event queues.

I make use of Qt pri's to better segment the codebase when used from QtCreator.  This makes it easier to gather common functionality together and provide structure to the overall project without having to segment the package into static libraries or lots of subdirectories.  It also makes for easy isolation and re-use of source components directly in other projects.

All headers should be documented to at least the header and class level.  I use doxygen to produce header documentation, and keep the docs at the bottom of the header file, so that the header code is pristine.  More about the code architecture and class layouts can be found using the ``make docs`` target.

## MacOS Specific Issues

For MacOS I presume the Digia online installer will be used to install Qt itself, rather than working with homebrew Qt or self-built configurations.  I do however presume that libeXosip2 will be installed with homebrew.  I will then bundle the Digia Qt runtimes for application deployment using macdeplyqt. 

On MacOS I will produce services as application bundles, and any additional libraries I may provide for use with Qt will be distributed as frameworks as well.  On GNU/Linux I use the distro version (for BSD, the ports collection version) of Qt, with both applications and any additional libraries installed in standard /usr paths by default.  On GNU/Linux I will use standard distro packaging (debian, rpm, etc) to make installable applications, and on both GNU/Linux and BSD the original source tarballs may also be directly used to produce and install my applications.

## Microsoft Windows

While the code base technically may still build and run on Microsoft Windows, and libeXosip2 can be built with MSVC 2013, there are features and functions going forward that I have neither time nor resources to test or develop for Windows.  Hence, it is depreciated and likely already broken as of this time.


## Project Planning

Official project planning is performed on the gitlab [sipwitchqt](https://gitlab.com/tychosoft/sipwitchqt) project.  I will be using the issue tracking system and kanban style issue boards offered there.  All issues will be added to the backlog there, with what they call milestones and sprints being interchangeable. 

I have standardized use of tags for issues in gitlab for project planning.  This includes "RC" for "release critical" issues, "hotfix" for bugs that may require an immediate release, "bugs" to distinguish bug issues from feature branches, and "blocked" to indicate some issue is currently blocked from further work. Standard tags can be found at gitlab [Tychosoft Tags](https://gitlab.com/groups/tychosoft/labels).

## Merges and Versioning

Feature branches should be created thru and named by the issue tracker directly.  This facilitates auto-closure on merge in gitlab.  Generally a collection of issues will be gathered together and put on a milestone for development and inclusion in the next x.Y.0 release.  All changes will be made on an issue branch taken from master, and merged back to master.  This of course is the normal gitlab issue flow.

While an active milestone sprint is underway any high priority fixes will be merged back to master immediately, and then cherry-picked into a hotfix-a.b branch as needed for a hotfix a.b.Z release.  Non-priority feature branches may not get merged until the end of the milestone sprint.  If there is no active milestone sprint underway, then any priority bugfix will be merged back to master and directly released as a point release as needed.

In addition to merge requests, a few things are done directly on master.  Tagging will typically happen from master, unless it is done for a hotfix patch release.  The changelog and version updates in the sources will happen as a direct commit to master.  Sometimes other packaging/release process issues will happen as direct commits on master as well.

As this project is still in rather early development, I am still be doing a lot of things
directly on master.  I am going to try avoiding this soon to better stabilize the development
process flow.

## Changelogs

For a long time I have used an automated script for producing changelogs.  This uses the summary line of the merge request.  This makes it important to create a relevant summary title for the merge request that is clear and concise.
