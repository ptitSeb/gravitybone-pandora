About **gravitybone-pandora**
=============================

This is the **OpenPandora** port of the freeware **Gravity Bone**

Compiling
=========

To compile, you'll need latest Codeblock PND.
A simple

  make release

Should be enough

The game is then compile inside quake2 folder.

It should be compilable on regular **Linux**, 
but you'll need to adjust the BASE_CFLAGS inside
the Makefile

Disclaimer
==========

Based on KMQuake2 by Knightmare
and includes Lazarus by David Hyde & Mad Dog.

Most code involves new gameplay objects in the game .dll. Note there's a fair amount of obsolete game objects that ended up not being used in Gravity Bone.

Minor modifications were made to the UI shell to remove the multiplayer menus, add support for checkboxes, and change some menu formating.

Also included is the GTKRadiant entities.def file used to make the maps.


Brendon Chung, 2008

Port by ptitSeb, 2015