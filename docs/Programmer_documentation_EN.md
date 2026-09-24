# Programmer Documentation

*Translated from the original Slovak PDF `Programmer_documentation_SK.pdf` (pdftotext + heading structure). Images and exact formatting from the PDF are not included.*

---

## 1 Portability
Marián Černý, Martin Košalko, Peter Knut


### 1.1 Project goal

The goal of the project was to create a game that would be portable to commonly used platforms:
Windows and Linux.

### 1.2 Tools used

#### 1.2.1 Graphics

To ensure portability we decided to handle graphical output using the OpenGL
library, which provides an interface for working with the graphics card. Portability of the application
with respect to input and window handling is handled by the GLFW library (An OpenGL Framework), which
supports many platforms. Besides the functions needed for window handling, the library
additionally offers a unified interface for working with threads, locks and condition
variables.

#### 1.2.2 Sound

For working with sound we decided to use the FMOD library. It is a commercial library
(free for non-commercial projects), but it is of very high quality. Its disadvantage is that it is available
only in binary form and only for some platforms. Of those interesting for our project, these are:
Windows 95+, Linux (IA-32) and Apple Macintosh.

### 1.3 Platform support

We developed and tested the project on these platforms: Windows 2000, Windows XP
and FreeBSD – all IA-32 (32-bit architecture). We did not develop the project directly on the Linux
platform, only tested it occasionally. However, the similarity with FreeBSD is so great that no
serious portability problems ever occurred. The compilers used were: MS Visual
Studio 6.0, MS Visual Studio 7.0 and the GNU Compiler Collection (GCC) in versions 2.95, 3.2 and
3.4.
Besides the platforms on which we developed the project, we tested these platforms: other
versions Windows 95, Windows 98, Windows Millenium, Windows XP AMD-64 (with the
executable in 32-bit form), Linux (IA-32).
The project should furthermore be compilable on other platforms supported
by the GLFW library. On the Unix platforms SGI IRIX, SUN Solaris, QNX, NetBSD,
OpenBSD, HP-UX and IBM-AIX no modifications should be necessary, or only
minimal ones. The same will apply to the MAC OS X platform, which is even supported by the
FMOD library (the library for working with sound). On the MS-DOS platform, modifications
of the functions for working with directories will be necessary. Otherwise compilation should be trouble-free, since
a GCC compiler is available for this platform as well. GLFW also supports the Amiga OS platform,
for which, however, no version of the GCC compiler exists, so compilation problems may occur there.
Compiling the project should also be possible on processor architectures other than 32-bit little-endian processors. The problem will be network play between different processor architectures;
it will only work between identical architectures. For network play to work also between
different architectures,
it is
necessary
to modify
only
two
functions:
the function


TEVENT::Linearise(), which prepares a message to be sent over the network, and
TNET_EVENT::Delinearise(), which correctly fills the TEVENT structure from a message
received from the network. However, we did not have other architectures available during development, and therefore these functions were not

implemented portably. That was not a goal of our project either.
Network play on the IA-32 architecture works without problems even across different platforms.
Combinations of Windows, Linux and FreeBSD were tested. AMD-64 in 32-bit mode (with the
executable in 32-bit form) also works.


## 2 Events – the game core
Martin Košalko


### 2.1 Introduction

The Dark Oberon project is not just a single strategy game, but a computational engine for real-time strategy
games, and the main program structures are adapted to this. Since the
program is a template for games, almost nothing can be assumed about the units, the environment or the maps of a concrete instance of the game.
Units can, however, be classified according to their properties into
predefined groups (combat units, workers, sources, buildings and factories). Each
of these groups can perform various specific activities (mining materials, producing
units). Some actions, however, remain common (attacking), although they may be
performed with minor differences (buildings can shoot, but they must not move). It
cannot be predicted, however, how fast the given actions will proceed. So that the computational engine itself
loads the system only minimally with respect to unit activity, leaving more room
for more demanding operations such as scene rendering, it uses a priority queue. Control
of unit activities is therefore handled by a system of events processed by the aforementioned
priority queue. Moreover, this structure is very well suited to the design of network
communication ensuring unit synchronization. Exactly those events are sent to the remote computer
which are enqueued in the queue on the local computer. The problem
of synchronizing unit activities over the network interface was thereby transformed (simplified) into
the synchronization of the local and remote event queues.

### 2.2 Event types

From the point of view of the computer and the event queue, players' units can be divided into local and remote ones.
Local units are units of players running on the local computer (in principle this can be
one real – human player and several computer players), and remote units are units of players
connected via the network interface. The idea of the event system is that the decision about which action a
unit will perform is always made on the local computer, while the result of the decision is also sent
to the queues of the remote computers, where these "results" are executed unconditionally. From this
point of view, events can be categorized as decision (planning) events – inserted only into the
queue of the local computer (local queue) – and action events, which are scheduled both in the local and in the
remote queues. It is thus automatically ensured that on all computers the
unit will perform the same action.
Each unit keeps its current state – the action currently being
performed – in its variable "state". Further decision processes and its
audiovisual manifestations – textures and sounds – are bound to the unit's state. In the program there are actions that can be described
by a sequence of unit state changes (walking = standing, moving, rotating, moving..., rotating,
standing), but sometimes it is necessary for a unit to outwardly perform one action while internally it must
be divided into smaller sub-actions (shooting = loading a projectile, aiming, firing the projectile,
waiting, reloading). From this point of view, events are divided into "events" – messages changing the state of the
unit – and "requests" – requests to perform some action. In the source code the event
numbers are defined systematically: US_XXX – event (US = Unit State), RQ_XXX – request.

### 2.3 Event description – the TEVENT class

A special object was created for events: TEVENT, which is a descendant of the class
TOOL_ELEMENT. As the name of the ancestor already suggests, all currently unused instances


of TEVENT objects are placed in a pool – the "event pool". The pool does increase
the memory requirements of the program, but on each insertion into the event queue it is not necessary to allocate
memory for a new event and on each removal from the queue to deallocate memory, which is unnecessary
and above all time-consuming. In the design we preferred time requirements over memory requirements –
instead of allocation and deallocation, an event instance is taken from and put back into the pool, which are
primitive pointer operations.
The TEVENT class contains:
•

address fields – unique identification of a concrete unit,
o player_id – unique player identifier (index into the array of players),
o unit_id – unique identifier of the unit within the player. Since units
are not in an array but in several linked lists (because their number is
highly variable), the conversion between a unit number and a pointer to the
unit must be fast (the conversion is, of course, performed for every event
taken from the queue). The conversion uses a hash table optimized
for the case where unit identifiers increase gradually. It is based
on the assumption that older units (with a lower ID) will be destroyed sooner than
units with a higher ID. Each event must therefore have a concrete recipient –
a unit.

•

handling fields – determine into which event queue and at what position the
event is to be (is) placed,
o priority – determines whether the event is to be placed into the priority queue. The existence
of the priority queue is a consequence of the fact that in some cases it is necessary that
two consecutive unit states cannot be interrupted by another state,
o time_stamp – time stamp at which the event is to be taken from the queue (according to
which it is, of course, inserted into it). Time is counted from the start of the game and all
computers have it synchronized. The field ensures that actions of the same
unit are performed in the correct order and, as far as possible, at the correct moment.
Due to the non-zero transmission time of events over the network interface it is not possible
in all cases to ensure the same order of execution of actions of two
units running on two different computers,
o queue_left, queue_right – link of the event in the queue (organized as a
doubly linked list).

•

data fields – information for processing of the event by the unit.
o event – event type (US_XXX, RQ_XXX). Processing proceeds according to the event
type,
o last_event, request_id – auxiliary fields which serve to refine
the processing of the event by the unit,
o simple1 – simple, int1, int2 – event parameters. They carry
all the parameters needed to perform the action (unit positions on the map, movement
target, attack target....).

### 2.4 Event generators

Events are enqueued, in principle, from four sources:


•

events generated by the real player – the real player controls his units with the mouse
and keyboard and thereby generates events passed to the units as information that
they are to start performing some action (i.e. "starting" events are generated),

•

events generated by a computer player – each computer player decides
about its units, from the point of view of events, identically to the real player. The only difference is
that it does not use the keyboard and mouse but neural networks,

•

events generated by a remote player – events enqueued into the event queue
via the network interface. As already explained, events arriving
from the network are "action" events and are executed regardless of local conditions,

•

events generated by a unit as a reaction to a received event – typically a
unit receives from the player only a "starting" event (for example move to position [X,Y,Z]).
The execution of this command itself is decomposed into elementary actions which the
unit plans itself. Events that the unit generates itself are both "action" events
(they are also sent simultaneously to the network interface) and "planning" events (they test
the availability of positions, the existence of other units ... and subsequently plan an "action" event).

From the above it is clear that it is necessary to ensure that a unit is "controlled" at a given moment
by only one event generator and thus does not receive contradictory events. The program solves this so that
a unit can have at any moment only one event of type US_XXX in the event queue
(an event changing the unit's state). This means that if a unit has scheduled in the event queue,
for example, the computation of the next movement position (i.e. it is currently moving between two
positions – mapels) and the user wants to start a completely different action with that unit (for example
an attack), the new action is scheduled only at the moment the previous elementary action ends –
for the event that is in the queue only the data fields are changed (the position in the queue and the time stamp
remain unchanged). There are, however, exceptions. An example is the destruction of a unit, which
must be performed immediately regardless of state-changing events in the unit's queue (which will
of course be ignored).
From the point of view of interruptibility, unit states can therefore be divided into interruptible (unit
regeneration), non-interruptible (unit movement between two mapels) and priority (unit
destruction).

### 2.5 Event processing

All event processing runs in a separate thread. The main event-processing function is
the function ProcessFunction(), which takes events from the queue in an infinite loop. At a given
moment all events whose time stamp is less than the current time are taken
(or none, if there are none in the queue). The event is then passed for processing to the appropriate
unit.
As already mentioned in the introduction to event processing, almost nothing can be assumed about the units of a concrete game instance.
However, according to the actions they perform, they can be
classified into predefined groups (attack units, workers, factories...). This is exactly how
unit types are represented internally as well. There is a hierarchy of unit types, which is
described in detail in the documentation of program structures, reflecting the existence of groups
with different properties. Some unit properties are common (units have
a view range), some are special for the given unit type (mining materials) and some are
similar (shooting of buildings and combat units). This fact is well implemented by
a system of virtual functions of unit types.


#### 2.5.1 The ProcessEvent function

The function ProcessEvent(..) is a virtual function of the unit type which processes events
taken from the event queue. From the point of view of event processing, the function is internally divided into two parts:
•

processing of action events (which are executed without any tests) – these
arrive both as reactions to the planning events of units and from the network
interface. This part has an important subpart – synchronization, where the
main properties of the unit are synchronized – position on the map and unit rotation,

•

processing of planning (decision) events (all conditions are tested
and the next unit state is planned).

Planning events of a local unit are always processed only on the local computer. Some
action events naturally have a "planning follow-up" (after a move, testing of the next
position must be scheduled for the unit), but that too is, of course, performed only for local units
on the local computer.
Processing of action events has an important subpart – synchronization. It ensures that
units on all computers have the same position on the map and the same rotation.
Due to the non-zero transmission time of an event through the network interface and the absence of a central
network authority (server), it may happen that a unit from a remote computer and a local unit
want to occupy the same position on the map. The program solves this by having two sets of coordinates
for units – real and local. The real ones are always the same on all computers, and the local ones are
always set so that no unit collisions occur on the local computer. For some
(with high probability short) time it may happen that the player sees a unit in the wrong
place, but with the next synchronization event the situation is, with high probability,
corrected and the coordinates are synchronized correctly. All action cycles of units (mainly
shooting, which requires the interaction of two units) are adapted to the existence of local
and real unit coordinates so that the local player does not notice any problem.

### 2.6 Description of unit action cycles

In all the diagrams that follow, the starting point – the player's intervention in the game – is
marked with a computer mouse symbol and the end point – the resulting event, or state – is
shaded grey.

#### 2.6.1 Walking

Walking as a whole is exemplary, because in the ideal case action
and planning events alternate in it. Moreover, if any problem occurs, requests are also
used. The ideal case of walking occurs when the unit, while moving between individual
mapels, does not bump into any other unit or into terrain impassable for it. Then
the states US_NEXT_STEP and US_MOVE (or US_ROTATING,
US_LANDING, US_UNLANDING) alternate. The state US_NEXT_STEP is a planning and testing one. As can be seen
from the diagram, it tests the availability and occupancy of the next position, the correct rotation
of the unit and several other things (unimportant for the algorithm itself).
If the next position on the unit's path is occupied by another unit, the unit waits
to see whether the way clears, and only after ten (defined by a macro) attempts does it compute another path
(the US_TRY_TO_MOVE cycle).


The path-finding method is also interesting. Path finding for a unit is, under general
conditions, time-consuming and therefore runs in another (pre-prepared) thread. So before the
path search starts, the unit is notified by the event US_WAIT_FOR_PATH that it is to wait for the
result of the computation (each computation has a special unique number). The unit remains in this state
(US_WAIT_FOR_PATH, of course, changes the unit's state) until it receives the event
RQ_PATH_FINDING (with the correct identifier) from the thread that was searching for the path. This event
starts the further movement of the unit.
All action events of walking – US_MOVE, US_RIGHT(LEFT)_ROTATING, US_LANDING,
US_UNLANDING are also sent via the network interface to the remote computers, where they are
executed.

#### 2.6.2 Attacking

Shooting, together with mining materials, is probably the most complex cycle, because in it
units of two different players must interact, which places increased demands on the
network interface. Three objects enter the cycle: the attacking unit, the projectile, and the defending
unit.
The task of the attacking unit is to get "within range" of the target and fire. In the ideal case,
therefore, the testing state US_NEXT_ATTACK and the action states (also sent via the network
interface) US_ATTACKING, RQ_FIRE_OFF (the projectile leaves the barrel) and RQ_FEEDING
(reloading) will alternate. In a less ideal case they are interrupted by path finding and moving after the target.
The projectile enters the cycle at the moment it is fired, which is captured by the event
RQ_FIRE_OFF. At this moment all the moments at which the projectile changes segment
(RQ_CHANGE_SEGMENT) are scheduled for it, and above all the moment and place of impact (RQ_IMPACT), which ends the life
cycle of the projectile. All events that are intended for the projectile are hatched in the diagram.
At the moment of the projectile's impact, on each local computer, local units
hit by the impact lose part of their life (the way of subtracting it is described exactly
in the documentation on automatic shooting), and a synchronization event RQ_SYNC_LIFE is sent to the remote computers from each such
unit. If a unit has zero
life (which is checked only on the local computer), the "destruction" of the unit follows –
a sequence of states US_DYING (dying), US_ZOMBIE (decomposing) and US_DELETE
(removal), which are also sent to the remote computers.

#### 2.6.3 Mining materials

Similarly to shooting, mining materials places increased demands on the network interface,
because units of two different players interact – the scheme player and the real
player who wants to mine. Two objects enter the cycle: the worker and the source. In the diagram the actions
and states of the worker are described above the horizontal line and those of the source below the line.
The main line of the worker is formed by the sequence of events US_NEXT_MINE, RQ_CAN_MINE, US_MINING.
The state US_NEXT_MINE is a testing and planning one. It tests the existence of the source (whether
someone has perhaps destroyed it), whether the worker is at the correct position...). If something is not right,
the worker tries to find another source in its surroundings and start mining from it. If everything is OK,
the worker sends the source a request to mine one unit of material – the event RQ_CAN_MINE
(?) and itself waits for the answer.


If the source has a free unit, it answers the worker with the event RQ_CAN_MINE (Y) (with the corresponding
question identifier), otherwise it answers RQ_CAN_MINE (N). If the amount of material
in the source changes, it is synchronized with the event RQ_SYNC_MAT_AMOUNT.
After receiving the answer, the worker reacts either by searching for a new source or by another mining
cycle. If the worker has mined the maximum amount it is able to carry, it finds
the nearest building that accepts the given material and carries it there.
After unloading the material (using the cycle of events US_UNLOADING and US_NEXT_UNLOADING) the
worker automatically tries to mine from the source it mined from last – it has it stored
in the variable source.

#### 2.6.4 Building and repairing

When building and repairing, two units of the same player must interact, and information
about both must also be sent via the network interface. All states described in the diagram
in the box with a dashed border are synchronization events of the unit being built as well as of the worker,
and they are sent via the network interface (on the local computer the necessary actions are performed
in the worker's states). Building and repairing are algorithmically completely identical and differ only
in details (the worker's state, which however has the same textures and sounds).
Both states begin with the worker having to get to the unit being repaired. If this fails,
the repair ends and the worker remains standing. Then only a cycle of planning
(US_NEXT_REPAIRING,
US_NEXT_CONSTRUCTING)
and action
(US_REPAIRING,
US_CONSTRUCTING) events runs, which gradually adds life and "progress" (only in the case
of building) to the unit being repaired. In case of successful completion, the event
US_STAY is sent to the unit being built.
All action events – US_CONSTRUCTING, US_REPAIRING, US_STAY are also sent to the
remote computers.

#### 2.6.5 Producing new units

Producing (training) new units takes place in factories. A factory can attack
foreign units (if it was defined that way), even while it is currently producing
new units. Unit production must therefore happen "in the background" – it must not change the state
of the building. That is the main reason why the event RQ_PRODUCING is used and not US_PRODUCING.
With each received RQ_PRODUCING event it is tested whether the player has enough materials, food
and energy, and accordingly the progress of the unit being produced is increased or not. In any case,
however, another RQ_PRODUCING event is scheduled in the event queue. As can be seen from the diagram,
synchronization events RQ_SYNC_PROGRESS are sent to the other players.
An interesting moment is when the factory has produced a unit but the unit cannot leave the building
(for example because the positions are occupied). Then the building schedules RQ_TRY_TO_LEAVE events
until the unit leaves the building.
After leaving the building, US_NEXT_STEP is sent to the unit on the local computer (so that it correctly
remains standing), but on the remote computers the unit is merely created at that moment (event
RQ_CREATE_UNIT).


#### 2.6.6 Source regeneration and unit healing

Source regeneration and unit healing have exactly the same principle, but they are applied to
different unit types and each increases a different property. Regeneration is applied to renewable
sources and increases the current capacity of the source, while healing is applied to movable
units (combat units and workers) and increases the unit's life.
As far as events are concerned, healing uses US_HEALING and regeneration US_REGENERATING. Neither
of these events is sent via the network interface to the remote computers, to which
the synchronization events RQ_SYNC_LIFE and RQ_SYNC_MAT_AMOUNT are sent instead. For remote
players only the consequence of regeneration and healing is important, i.e. that the
capacity of the source, or the life of the unit, has increased.
Unit healing is performed only if the unit is standing or has landed
and at the same time has the property RAC_HEAL_WHEN_STAY, or RAC_HEAL_WHEN_ANCHOR, which
are specified in the configuration file. The times when the unit was standing are summed (scheduling of
US_HEALING events is adapted to this).
Source regeneration takes place only if the source is renewable. The first unit of material
(after the source has been completely mined out) is added at a different time than all other units
of material. Regeneration always takes place when the source is not at full capacity.


### 2.7 Diagrams of unit action cycles

#### 2.7.1 Walking

US_LEFT_ROTATING
US_RIGHT_ROTATING

+
Rotation
required

US_NEXT_STEP

–
RQ_PATH_FINDING

US_WAIT_FOR_PATH

+

Is the terrain
of the next
position
passable

–

Path exists
*

–

+

Is the next
position free

US_MOVE
US_LANDING
US_UNLANDING

+

–

# 10x
*
> 10x

US TRY TO MOVE

* Planning a new path
US_STAY
US_ANCHORING

+


Last
position

–


#### 2.7.2 Attacking

US_START_ATTACK

US_NEXT_ATTACK

Does the
target
unit exist

Movement to the target unit

–

US_STAY
US_ANCHORING

+

Is the target
unit within
range

US_END_ATTACK

–

US_NEXT_STEP

+
US_ATTACKING

RQ_CHANGE_SEGMENT

RQ_SYNC_LIFE

RQ_IMPACT

RQ_FIRE_OFF
RQ_FEEDING

End

–

Is the target
unit
destroyed

US_DYING

US_ZOMBIE

US_DELETE


#### 2.7.3 Mining materials
US_START_MINE

US_NEXT_STEP
Movement to the source
–

US_STAY
US_ANCHORING

Is the worker at
the source

Unloading
material
+
Movement to the building

US_NEXT_MINE

US_NEXT_STEP
+
Building
found

Is the source
destroyed

–

+

Searching for
a new
source

+
Does a new
source exist

–

–

Searching for a building
accepting the material

US_STAY
US_ANCHORING

US_SEARCH_NEAREST
+
Is the worker
full

–

US_MINING

RQ_CAN_MINE (YES)

RQ_CAN_MINE (NO)

RQ_CAN_MINE (?)

RQ_SYNC_MAT_AMOUNT

+

Can the source
provide
material


–


Repairing and building

*
Does the unit still exist?
Is the unit in the correct state?
Does the unit still need repairing?
Do I have enough materials?
Do I have enough energy and food?

US_NEXT_STEP
Movement to the unit
being built
(repaired)

US_START_REPAIR

RQ_CREATE_UNIT
RQ_UPGRADE_UNIT

+

at the unit

US_STAY

+

The unit is
repaired

US_REPAIRING
US_CONSTRUCTING

The worker is

–

US_NEXT_REPAIRING
US_NEXT_CONSTRUCTING

+

Is everything
OK *

RQ_SYNC_PROGRESS
RQ_SYNC_LIFE


–

(The unit is unavailable) –

#### 2.7.4 US_STAY
US_ANCHORING


#### 2.7.5 Producing a new unit
RQ_PRODUCING

–

Done
+

End

+

Can the unit
leave the factory
–
RQ_CREATE_UNIT
US_NEXT_STEP

RQ_TRY_TO_LEAVE


RQ_SYNC_PROGRESS


## 3 Structures
Jiří Krejsa, Peter Knut


### 3.1 Introduction

This part of the documentation describes the basic data structures and the relationships between them.

#### 3.1.1 Unit structures

From the point of view
of units,
two types of structure
are key.
The first of them describes the properties
of a kind of unit, which are
denoted
by the word
ITEM,
and the second keeps information
about the concrete
instances
of these kinds. These instances
are denoted by the word UNIT.
Of course there are various
types of unit kinds, which
together
form
a tree
hierarchy. From the above it
follows
that
there are
two
tree hierarchies of classes.
Let us now look briefly at
the meaning of the individual classes
(the tree structure is shown in
figure no. 1):
•

TDRAW_ITEM – this class is

the cornerstone of the
hierarchy,
it contains
the information needed for
rendering
units,
•

Figure 1: Class hierarchy of unit kind types.

TPROJECTILE_ITEM –

the class stores the properties of a projectile,
•

TSURFACE_ITEM – the class is used for objects that are part of the map. It stores

local changes of the surface,
•

TMAP_ITEM – the class is the common ancestor of all objects that can stand on the

map,
•

TSOURCE_ITEM – the class stores the properties of sources,

•

TBASIC_ITEM – this is the common ancestor of buildings and units. The player can
manipulate descendants of this class,

•

TFORCE_ITEM – the class stores all the necessary information about movable

units. Everything the player can move on the map is an instance of this class
or of its descendant,


•


TWORKER_ITEM – an extension of movable units enabling mining of materials,

building of buildings and repairing of units,
•

TBUILDING_ITEM – stores the information needed for buildings,

•

TFACTORY_ITEM – an extension of a building enabling production of movable units.

The meaning of the classes is exactly the same as the meaning of the unit kind types. So let us mention
briefly only the class that is additional in the hierarchy.
•

TPLAYER_UNIT – additionally contains information about the unit being owned by a player.

Figure 2: Class hierarchy of unit kind instances.

#### 3.1.2 Map structure

Two kinds of map are used in the game:
•

global, which contains complete information about the game map,

•

local, which differs for each player.

##### 3.1.2.1 Local map

The local map maintains three kinds of information that have a common property. They concern
the map, and each player can have this information different from other players. The map is


represented by a three-dimensional array of TLOC_MAP_FIELD structures. Each field of the local
map has three important attributes:
•

state – expresses the visibility of the field in the global map for the owner of the local map. Three
kinds of values are distinguished:
o WLK_UNKNOWN_AREA (=255) – unknown area, the player knows no
information about the field,
o WLK_WARFOG (=0) – expresses that the current state of the field is unknown,
o other positive numbers (>0 and < 255) – mean full knowledge of the current
information about the state of the field, i.e. that it is within sight of at least one of the player's units.
The value of the number expresses the number of the player's units that see the given field.

•

terrain_id – terrain kind. Again three basic types are distinguished:
o WLK_UNKNOWN_AREA (=255) – the terrain is unknown to the player; this situation occurs
only if it is at the same time an unknown area for the player,
o non-negative numbers less than one hundred – according to the player's information, the field
with this terrain_id has terrain of the given type, where the type values correspond
to the values used in the global map,
o non-negative numbers from one hundred to two hundred – according to the player's information, the field
with this value has terrain of type "current value – 100", on which a building stands.

•

player_id – expresses the id of the player who is on the field according to the player's information. It distinguishes
two kinds of values:
o WLK_EMPTY_FIELD (=255) – no player stands on the field, i.e. the field is
empty,
o another value – expresses the player ID; this ID equals the index in the global array
of players.

##### 3.1.2.2 Global map

Contains information corresponding to the real state of the game. Every computer connected to the
game (in a network game) has a copy of it. These copies are synchronized using events that
units exchange.
The global map (TMAP) contains above all an array of three segments, an array of three lists
of units for each segment and a structure managing the warfog. Each segment
(TMAP_SEGMENT) further contains arrays of fragments, objects and layers belonging to that
segment, and a two-dimensional array with the surface of the segment. The segment surface (TMAP_SURFACE)
includes three items for one map field:
•

terrain identifier,

•

the unit standing on the field,

•

activity for each player. This activity is used for the decision-making of the computer
player.

The unit lists for each segment serve for sorting and rendering units.


#### 3.1.3 Player structures

There are two types of players: an ordinary (human) player represented by the class TPLAYER
and a computer player represented by the class TCOMPUTER_PLAYER. Each player contains
information about its state, a list of units that belong to it and tools for path finding.
The computer player additionally has information and tools for decision-making using neural
networks.


## 4 WALKING
ALGORITHM
Valéria Šventová


### 4.1 Introduction

Walking of all unit types is handled by calling the function PathFinder(..). This
function receives via its parameters information about the unit that is to
move (a pointer to the given unit), information about the terrain (a pointer to the local map) and also
the coordinates of the target. On output it returns the found path and possibly a new target. The new target is the map
field that is closest to the original target of the path and is always reachable by the unit (the unit
can walk to it). An example is sending a unit to an unreachable target (for example one
surrounded by rocks, or by terrain which the unit cannot get around in any way to reach
the target.).
How a little man can be made to move is described in the user manual
for the game Dark Oberon (user documentation).
Two types, or ways, of walking are distinguished in the project:
•

walking of an individual,

•

walking of groups.

In both cases a modified A* algorithm is used, described in the following
paragraph.

### 4.2 A* algorithm – the principle of operation

The A* algorithm serves to find the shortest path on a map. Its performance is high and it is one of
the best search algorithms.
The input information is:
•

knowledge of the map size (the map must be divided into fields),

•

the passability of all its fields (we divide fields into passable and impassable),

•

the coordinates of the start and target of the path.

#### 4.2.1 Determining the path

To determine the path, the individual fields need to be evaluated. The evaluation is determined from the relation:

F = G + H (+D)
where:
F – distance coefficient
G – distance of the currently evaluated field from the start field
H – distance of the currently evaluated field from the target field
D – difficulty of the field
The reason for such an evaluation lies in the fact that a shorter path is considered better.
NOTE: The basic and simplest A* algorithm works on a map all of whose
fields have the same difficulty, which is why we give D in the formula above only
in parentheses.


#### 4.2.2 Structures used

The A* algorithm uses 2 basic sets:
•

Open set – organized as a heap, contains all nodes (map fields) that have already
been evaluated by the algorithm but have not yet been selected into the final path

•

Close set -- organized as a sorted list, contains all nodes that have already been
evaluated and have been selected into the target path. Each element of this set was selected
from the Open set as the element with the minimal evaluation.

#### 4.2.3 Operation of the A* algorithm

The A* algorithm works as follows:
It adds the start field to the Open set (the G value is 0). A loop is performed over all fields
of the Open set until it is empty. If the algorithm ends in this
way, the path to the original target has not been found. In the mentioned loop the following is performed:
•

Select the minimal item from Open (in the case of a heap this item is at the root).

•

Add the selected item to the Close set

•

If this item is the target field, the algorithm ends, the path has been found.
Otherwise all neighbouring fields of the selected
field are evaluated and added to the Open set (i.e. inserted into the heap)

### 4.3 Modifications of the walking algorithm

In the Dark Oberon project the A* algorithm is used with several minor changes, mainly
in order to increase speed or prevent possible infinite looping.
1. The game uses a system of 3 horizontal segments in which the given units
can move (if they have such an ability defined in the configuration
file). It was desirable for a unit to be able to pass from one segment to
another if a path composed this way is more advantageous for it. For this reason, for each
field inserted into the Close set not only the neighbours in the 2D sense are evaluated
(i.e. those whose x, y coordinates differ from the given field by +1 or –1), but neighbours in the
3D sense (the third coordinate is defined by the segment).
2. The evaluation of fields is more complicated than in the basic version of the A* algorithm.
There are several reasons: the use of warfog (fields not yet discovered receive
a special evaluation), different unit types (each unit type has its own defined
range of terrain that is admissible for the unit. Fields with terrain outside this
range are inaccessible for the unit; moreover, individual unit types may move at different speeds on a given
surface), etc. Fields in the map are divided into:
a. Admissible for movement – these are fields that are not outside the map, the unit
may enter these fields, and the field is not occupied by an enemy unit
(denoted moveable in the project),
b. Admissible for landing - fields that are not outside the map, are not occupied
by an enemy unit, and the given unit has them in its list of fields on
which it can land (denoted landable in the project),
c. Inadmissible - not allowed for the unit.

Let us call the field whose neighbours we are trying to evaluate the central field.
Then the evaluation of a neighbouring field is given by the sum:

C + D + H
where
C – the real distance of the central field from the start
D – diagonal distance: depends on the position of the field relative to the central
field.
Let: a – terrain difficulty, s – unit speed in the segment. Then
D=a*sqrt(2)/s if
the
evaluated
field
lies
in a diagonal direction from the central field, D=a/s if the field lies in a
straight direction from the central field, D=2*a/s if the field lies in the
vertical direction from the central field.
H – heuristic estimate of the distance of the given field from the target
NOTE: for fields where landing is possible, the diagonal distance is additionally multiplied
by a penalty constant which expresses the difficulty of the landing manoeuvre.
A landing field can be used only for the last step of the path.
3. Target fields: In the A* algorithm the target was always represented by only one field.
In the Dark Oberon project the target area is modified in various ways depending on whether
the user marked another unit as the target, or just a field on the map on which no
unit stands. In the second case there is only one target field. In the first case the
target area contains the fields under the unit the user clicked on and additionally
all fields around this unit up to such a distance that the unit for which the
path is being searched would be adjacent to the unit the user clicked on. The unit is therefore
at the target if its lower-left corner reaches into the target area.
(Figure 3).

Figure 3: Target set for a moving unit of size 3x3

4. Target: in the Dark Oberon project, during path finding the field that is
closest to the target of all fields of the close set is remembered. This field is used as a
substitute target in case the original target is
unreachable for the moving unit.
5. Unlike the classic A* algorithm, besides the Open and Close sets a
Path set is also used, which contains all fields that will appear in the resulting
path, if finding this path is successful.


### 4.4 Walking of units

As already mentioned in paragraph 1, walking of a single unit and
walking of a group of units are distinguished.

#### 4.4.1 Walking of an individual

The algorithm for finding a path for a single unit is a modification of the A* algorithm. If
the unit has some path computed from a previous call of the PathFinder function, this
path is destroyed and replaced by a new one.

#### 4.4.2 Walking of a group of units

When searching for a path for multiple selected units that form a formation, PathFinder
is not called for each unit of this formation separately. The whole formation is divided into one or
more groups (this division is handled by the function GetGroup). The division into groups
proceeds by always taking the first unit from the unit list and finding for it all
units of the same type which are not too far away from this unit. At the same time
these units are removed from the unit list, so that they are no longer considered when searching for the next group.
In one such group a so-called leader is found, i.e. a unit for which a path to the
target exists. For each unit of the group its offset relative to the leader is then computed,
and the path of this unit equals the leader's path plus the corresponding offset. If some
unit of the group encountered an obstacle when attempting to move along the "shifted path",
the unit computes a path from this obstacle to the target independently of the other units
of the group, by calling the PathFinder function.
The reason for such a division into groups was the effort to increase the speed of path computation for
a group.

### 4.5 Return values of the PathFinder function

As already described at the beginning of this documentation, the PathFinder function returns a pointer to the
found path (path), and the real target, i.e. the field closest to the original target of the path which is, however,
reachable for the unit. The found path and the real target are parameters passed by reference.
The PathFinder function returns a boolean type as its return value. It returns FALSE if:
•

The input parameters are not admissible (the pointer to the moving unit is NULL
etc.)

•

The target and start fields are identical

•

A path for the given unit could not be constructed

If none of the mentioned situations occurs, the PathFinder function returns TRUE.

### 4.6 Walking and implementation in threads

The whole walking process was implemented in threads in order to increase the speed of path computation.
The game runs in the main thread; for path computation a waiting thread is taken from the thread pool
and is given the task of finding a path for the given unit. A more detailed description of the overall


operation of threads can be found in the section on the ProcessEvent function, or in the section
on the thread pool.

### 4.7 Implementation details of the PathFinder function

The data and methods for the modified A* algorithm are encapsulated by the class TA_STAR_ALG. It keeps
a pointer to the so-called star_map, into which field values are recorded during the path-finding
process and which is implemented as a pointer to the class TA_STAR_MAP. This class already
represents the map itself in 3D (width, height, segments). The map for a given unit is allocated
under the following conditions:
•

The given thread executes the PathFinder function for the first time, i.e. it has no allocated
star map fields

•

The current map does not have the corresponding dimensions - in this case this map is freed
and a new map with the required dimensions is allocated. This situation occurs when
the thread executes the PathFinder function for the first time in a new game and another game with
a differently sized map has already been played before in this run of the program

SUMMARY: From the above it follows that this is a lazy implementation and allocated
maps are carried over between different game runs (note, not application runs). If the
already allocated map is admissible, it is only cleared (function ResetMap).
Besides the star map, the class TA_STAR_ALG also keeps information about the Open and Close sets
(open set, close set, path set). The Open set is organized as a heap in an array,
the minimum is extracted by the function ExtractMinOpenSet, insertion into the Open set is the task of
InsertToOpenSet.
The Close set is an array with dimensions [number of segments*map width* map height +1]; it is allocated
dynamically in the constructor of the class TA_STAR_ALG. A filled field of this array points
with a pointer to some field in the star map. At the same time, the given star map field keeps in the variable
p_heap_fld the address of the given field in the structure into which the field was placed
(Open or Close set).
This situation is best described graphically by figure 4 (it captures a situation in which a
map field points to a field of the Close set, but it can equally point to a field of the
Open set):

Figure 4: Connection of the star map and the Close set


Such a structure was chosen mainly because of the need to construct the resulting path quickly.
The resulting path is constructed by the method CreatePathList of the class TA_STAR_ALG. This function
receives the target and start of the path as parameters and returns as its return value a pointer to the first field
of the path, or NULL if the path cannot be constructed. At the beginning, first an
instance of the class TPATH_LIST is created, which encapsulates the methods and variables concerning the
resulting path.
The structure holding the resulting path is described by figure no. 5.:

Figure 5: PathList

NOTE: An instance of the TPATH_NODE structure is created after the already existing
instances are filled, i.e. after all elements of the path_pos array are filled (shown in the figure in dark
grey). The newly created instance is placed at the beginning of the list. Since
the path is constructed from the target towards the start, the position of the start and target
fields described in the figure is logical. It is likewise evident from the process of creating this list that
the path_pos array of the first element of the list may remain partially unfilled. Therefore, for reasons of
speed, a separate variable first was used to store the first filled field.
The reason for such a structure was the fact that the path for a given unit in the vast majority of cases
does not require the creation of more than one instance of the TPATH_NODE structure (which holds up to 256 steps
of the path), which is economical in terms of allocation. Moreover, using this structure gives good results
in terms of speed.


## 5 Attack and defence
Jiří Krejsa


### 5.1 Introduction

Although some parts of the code related to the attack phase of the game must be written differently
for different unit types, it is possible to find a common set of properties and behaviours. Before
diving into the implementation details, let us look at the idea itself.
The first operation the attacking unit must perform is to verify the possibility of attacking, i.e.
for example to check whether the selected target is within firing range, etc. After verifying
that the target is reachable, the firing phase follows, which is represented for example by the movement
of a catapult arm or the swing of a sword. After firing is finished, logically the release of the
missile follows. Here events split into two storylines. The first of them are the activities that follow from
the point of view of the attacker, who starts reloading. The second view is the actions performed by the
missile itself. From the moment of release, the projectile becomes independent of its originator, i.e.
the attacker becomes just an ordinary unit that can be hit. The first activity the
missile must perform is reaching the impact position. After covering the necessary distance,
the impact itself occurs, which includes a detonation, as a result of which the surroundings of the
impact are hit. For all objects in the hit area the moment of evaluating the effect of the
missile comes. That is, the total strength of the hit is determined as the strength of the weapon, partly randomly
modified, divided by the number of fields in the impact area and by the quality of the unit's armour,
and adjusted by the product of the ability to take cover and chance on the defender's side.

### 5.2 Implementation

Providing the required functionality is divided into two logical parts. The first area is
passing through the individual phases of the attack and the second is the implementation itself. The first area is
controlled from the ProcessEvent method, from which the methods providing the actual activity are called.
Now we will discuss primarily the second area, i.e. the deeper details of the implementation. Most of
the source code is located in the files dofight.cpp and dofight.cpp.
Every object placed on the map, i.e. an instance of the class TMAP_UNIT (typically of descendants
derived from it), has an instance of the class TARMAMENT, which contains a separate attack
and defence part. Each instance of this class contains a defence part, i.e. the class TDEFENSE.
The attack part of the armament, which is represented by the class TGUN, is optional, which
expresses the ability or inability to attack other units independently. The only
purpose [of the defence part] is to store information about the quality of the unit's armour and the ability to avoid the
consequences of a hit.

#### 5.2.1 Hittability test

The key part of an attack is the initial test of whether the given target can be attacked immediately. Because
most of the computations needed to carry out the attack must already be performed when verifying the availability of the target,
the result of the test is not only a value indicating whether an attack is possible, but also information about what
may need to be done to make the target reachable. At the same time the real target
of the missile is determined, which differs from the planned one by taking the weapon's inaccuracy into account. All this information
is returned in the TATTACK_INFO structure by the method IsPossibleAttack. Let us now discuss
in more detail what is done in the method testing the possibility of an attack. First
several simple tests are performed - whether the target is in a hittable segment, whether I do not first have to
end anchoring, and whether I am able to attack this type of opponent, where we distinguish three
kinds of objects – buildings, sources and others. Another condition to test is whether the target is


within range, which holds if at least one of its fields is within range. The distance here is
tested using the three-dimensional Pythagorean theorem, where the third dimension is represented by segments.
A special case are units with range exactly one, where all eight surrounding
fields are considered to be within range. If the target satisfies all surrounding
conditions, first the ideal hit point is computed and on its basis the actual place
of impact, which is modified according to the weapon's inaccuracy.

#### 5.2.2 The attacker's attack cycle

In this paragraph we present the course of an attack from the point of view of the attacker and the events it sends or
to which it reacts. This cycle is in principle similar to the other action cycles such as mining,
walking or building. The event that starts attacking is US_START_ATTACK, which correctly
ends the actions being performed, remembers the target and sends the event US_NEXT_ATTACK. That one tests whether
the remembered target is not dead, dying or whether it has possibly escaped from sight. If so,
it sends the event US_END_ATTACK, which takes care of ending the attack, and if not, it performs the
hittability test. Depending on its results and the type of attacking unit (movable or
immovable), it either starts attacking, tries to take a better position or ends the attack.
If an attack occurs, the unit passes into the state US_ATTACKING, during which an
instance of the projectile (TPROJECTILE_UNIT) is prepared and the attack manoeuvre begins, which means for example
the movement of a catapult arm or a sword swing. At the same time a request to release the
projectile at the correct time is sent. All these actions are performed in the method FireOn. Even though
firing and reloading are two different actions, the unit remains in the state
US_ATTACKING after both have been performed. After the time reserved for firing and reloading has elapsed, the unit passes
back into the state US_NEXT_ATTACK.
As mentioned in the previous paragraph, during the state US_ATTACKING the
unit should process the request (RQ_FIRE_OFF) to release the missile. During
processing, the projectile is displayed and any requests for a segment change
and requests with the impact time are sent. All these requests are directed at the projectile instance.

#### 5.2.3 Evaluation of the projectile impact

First let us briefly go through the sequence of activities bound to the projectile. After the
projectile is released into space, it starts moving towards the place of impact. If the trajectory of the missile
passes through several segments, their change is signalled by the request
RQ_CHANGE_SEGMENT. The moment of impact is signalled by the request RQ_IMPACT.
The reaction to it is invoking its own method Impact, which takes care of resolving the impact and
freeing the projectile instance from memory.
The above-mentioned method traverses the surroundings of the impact position within the range of the explosion, the so-called
impact area. If there is a unit on any of the fields, the intensity of the hit
it suffered is computed according to the following relations:

DN = PrC * DPr
where:
PrC – a random number from the interval [0.4 ; 0.6]
DPr – the defence ability of the kind of the hit unit
Then DN is the defence number expressing the chance of reducing the damage


w = (PMin + (PMax – Pmin)*PoC)/CHM
where:
Pmin – minimum attack strength of the attacker's weapon
Pmax – maximum attack strength of the attacker's weapon
PoC – a random number from the interval [0 ; 1]
CHM – number of fields in the impact area
And the result w gives the extent of injury according to the quality of the attacker's blow

W = (w / DA) * (1 – DN)
where:
DA – armour quality of the hit unit
DN – defence number computed from relation 1
w – result of the previous relation
The result of this relation is W – the actual injury of the hit unit

#### 5.2.4 Handling an attack without a projectile

So far in the description we have talked only about ranged attack, i.e. one in which the injury
is caused by a projectile fired by the attacker. We have not talked about face-to-face attack, i.e.
for example attack with a sword or fist, or a club. The apparent omission, however, had
a simple reason - this non-shooting attack is also handled by exactly the same mechanism as
described above. The only difference is that the "projectiles fired" by a sword are not visible,
because they have no texture and their flight time is zero. Or - that the time of their impact is
identical to the time of release of the projectile by the attacker.
The main advantage of choosing this solution is the absolute uniformity of handling
the attack activity, i.e. also saving the need to create further code which, apart from the placement
of methods, would fully match ranged attack in its key parts.

#### 5.2.5 Automatic defence

The spontaneous defence of one's own units against the enemy is closely related to their set
aggressiveness. As mentioned in the user part of the documentation (specifically
in the user manual), the aggressiveness of units' behaviour can be set, in a range of
four levels – stay, guard, offensive guard, agressive guard. The basic idea that
the defender attacks an intruder of its space upon finding it is reversed, i.e. the intruder reports
the intrusion into its space to the defender. This method is achieved by maintaining
auxiliary lists for each map field. The lists are of two types:
•

The first expresses the view range of units,

•

The second expresses the firing range of units.

So when changing position, the unit sends an event to all units that satisfy all
of the following conditions:


•

They have the aggressive mode set (agressive guard) and the intruder has come within their view range,
or they have the defensive (offensive guard) or cautious mode (guard) set,
and in both of these modes the intruder is within their firing range,

•

The intruder is not a unit of the hyperplayer (the player owning map objects such as pigs
or trees that are not a source of material, etc.),

•

The defender does not have another unit as its target.

The lists are implemented with the class TMAP_POOLED_LIST, which is a descendant of the class
TPOOLED_LIST. It is a simple singly linked list which does not allocate its elements
but uses ones pre-allocated from a "pool". The selection of units that will start
attacking is performed by the method AttackEnemy, which distinguishes the class TMAP_POOLED_LIST from its
ancestor. As mentioned above, it is not the defender that takes care of calling it, but the intruder.


## 6 Thread pool
Jiří Krejsa


### 6.1 Implementation of the thread pool

#### 6.1.1 Use of the thread pool

The thread pool is used in two places. The first is path finding for
units and the second is measuring the distance between a unit and a source, a unit and
a building, etc. Let us now look at the implementation itself in more detail.

#### 6.1.2 Implementation details

The interface for using the thread pool is in the file dothreadpool.h, and because the pool is
programmed as a template, this is also the location of the function definitions. Everything needed is
encapsulated in the class TTHREAD_POOL. First let us briefly look at how the pool is used.
The basic idea is to create an interface that ensures reliable access to multiple
threads without the user (in the sense of a programmer who wants to use the template) being forced
to deal in any way with thread occupancy problems, etc. Therefore the idea of picking
threads is moved into the background and replaced by the idea of posting requests to the thread pool.
Each of these requests is added to an input queue on which a
condition variable is created. Inserting into the queue wakes up one of the prepared threads, which
takes the request and resolves it. The computed result is stored in an output queue of solutions,
from which these solutions are taken in order. Before diving into the details
of the implementation, let us mention one more idea. Because auxiliary
data is often needed for the computation, on which mutual exclusion between threads would have to be guaranteed, this data is made
available separately to each individual thread. With the permanent addition of a class that
encapsulates the auxiliary data, the possibility of posting requests was also extended. Each
posted request is accompanied by information about the method to be used to compute the
results. Any member method of the auxiliary class whose type matches can be chosen.
Let us now discuss the solution used in detail. The template header is as follows:
template <class I, class O, class A> class TTHREAD_POOL {…}

The meaning of the three arguments is simple. The first parameter I denotes the type that contains the
information needed to perform the computation. A simple rule applies that one instance
corresponds to one request. The second parameter O is the output value that is inserted into the
response queue. Finally, the third parameter A is the type that encapsulates the auxiliary data. Each
thread will work on its own instance of this type.

##### 6.1.2.1 Creating the pool

The only way to create an instance of the thread pool is to use its static method
CreateNewThreadPool.
static TTHREAD_POOL<I, O, A>* CreateNewThreadPool(
int thread_count,
unsigned int req_queue_size = THP_QUEUE_SIZE,
bool use_res_queue = false,
unsigned int res_queue_size = THP_QUEUE_SIZE)


The meaning of the first parameter does not need much discussion. It determines the number of threads in the pool.
With the second parameter we pass information about the expected size of the request queue. If
we do not specify a value, a queue of up to 100 requests will be expected. The value is used
to pre-prepare the required number of instances of the class wrapping the requests. The third
parameter says whether the output response queue will be used. If it has the value false,
the queue is not used and the results of the functions are discarded. The fourth and last parameter is the
size of the output queue and is meaningful only if the response queue is used. The method
returns a pointer to the created thread pool on success and the value NULL if an error
occurred.
The creation process itself can be described very briefly. The input queue is created and, if
requested, also the output queue. Then a condition variable is prepared and
an array of instances of the class TTHREAD, which contains an instance of the auxiliary data, is created. At
the very end the threads are prepared. Each thread is started in the method
FunctionStarter, which handles taking requests, starting the computation and inserting the
response.

##### 6.1.2.2 Adding requests

As already mentioned, the pool is worked with by adding requests. To post a
request, the interface of the following method is used:
unsigned int AddRequest(I *request,O* (A::*processor)(I*))

Let us look at the meaning of the individual parameters. The first parameter is an instance of the type that
contains all the information needed to start the computation. The second parameter is more interesting; it is
a pointer to a member method of the class that contains the auxiliary data for the computation and an instance of which
each thread holds. From the header it can be seen that it receives a single parameter, namely a pointer to the input
type, and its return value is a pointer to the output type.

##### 6.1.2.3 Taking responses

If the output queue with responses is used, the results need to be taken out. For that
the method O* TakeOutResponse() serves. The method has no parameters, so it only remains
to note that the results are returned in the order in which they were returned by the computing
methods.

##### 6.1.2.4 Destroying the pool

All operations needed to destroy the pool are performed in the destructor, which, unlike the
constructor, is public. First all threads, including those that are performing a computation, are terminated.
Then the condition variable and the input and output queues are freed.


## 7 Computer player
Michal Král


### 7.1 Introduction

Implementing the behaviour of the computer player was made very difficult by the great generality of the game.
Unlike most strategy games, in which the individual buildings and units are fixed,
in the game DarkOberon the user can define diverse types. To at least partially
deal with this problem, neural networks are used in some places
instead of a purely algorithmic solution. From a general point of view, the decision-making of the
computer player works on the basis of dividing the map into smaller parts, which are all regularly
evaluated (their importance, the decision what to do and where) and an action is performed using units close to the given
area.

### 7.2 Neural networks

Most of the evaluation is done with the help of neural networks. For simpler
implementation only one type of network was chosen, and for the individual cases only different
numbers of neurons in the individual layers are used. All networks used in the DarkOberon project are
perceptron networks, they have only one input and two hidden layers, and only one neuron in the output
layer. The connection between the individual layers is made in an all-to-all style (every
neuron of the input layer is connected to every neuron of the first hidden layer, etc.) as
figure no. 6 shows.

Figure 6: Neural network

As the activation function a sigmoid is used, more precisely its variant with the following formula
1/(1+exp{-4*Σ[neuron input]})
and the numbers of neurons in the individual layers are chosen rather so that the network learns the examples than
according to some rule. The Back-Propagation algorithm was chosen for training the networks, but
no training takes place within the game at this stage; it was, however, used when creating the networks for the game
and is implemented. For this algorithm the final version uses the variant with a fixed
step coefficient (the so-called η) with the value 3. Variants with a dynamic value as well as
other values were tried, but this one proved to be the best and simplest. All networks are
stored in the directory Nets and their names are fixed by the program code, but their storage format is
readable. The file is divided into lines, and in them the individual values are separated by a space. Numbers


denoting the number of neurons are written without a decimal part (format %i), numbers with a decimal
expansion are stored in the %e format, i.e. with a comma as the decimal separator and without
use of an exponent. On the first line there are, in this order:
• Number of neurons in the first layer (input neurons),
•

Number of neurons in the second layer,

•

Number of neurons in the fourth layer (output neuron),

•

Threshold value (common to all neurons in the network),

•

η (step coefficient in the Back –Propagation algorithm.

The further lines each correspond to one neuron, from the first input one, through the second input one, up
to the last output neuron. On a line there are then the weight values of the inputs into the neuron, e.g. for
a network with topology 4-7-5-1 the 15th line will correspond to the 3rd neuron of the third row and there will be 7
values corresponding to the weights between the 1st neuron of the second row up to the 7th neuron of the second row
and this 3rd neuron of the third row.
An example of such a file for the network from figure no. 6:
4 3 2 1 -4.316021e+008 3.000000e+000
-9.180000e-002
-6.600000e-003
-3.320000e-002
0.000000e+000
-6.620000e-002

4.480000e-002 -4.400000e-003 -2.840000e-002

9.240000e-002 -7.200000e-003

4.100000e-002 -7.100000e-002

-4.380000e-002

6.540000e-002

9.220000e-002 -1.800000e-003

9.900000e-002

8.840000e-002

6.540000e-002

-1.280000e-002 -2.180000e-002

2.080000e-002

8.040000e-002 -6.940000e-002

The use of the networks will be explained later in the context of the whole decision-making process of the computer
player.

### 7.3 Evaluation of races

Even before the game itself starts, when the races are loaded, an evaluation of the individual units
takes place using neural networks. For each unit first the strength of its weapon is evaluated
and then its overall usefulness. Units and buildings are evaluated separately, but
a single neural network is used for determining strength. First the neural network from the file
network_for_armaments is used. Almost all properties of the unit's weapon are given to it as inputs.
Once the weapon is evaluated, the whole unit is evaluated. Here two
different neural networks are already used – one for movable units (network_for_force) and the other for
buildings (network_for_building). For units the input values are maximum speed,
view range, energy, food, maximum life, flags whether the unit has the ability to build,
repair and mine, weapon strength and the like. For buildings they are, for example, the evaluation of the best
unit the building can build, the number of accepted materials, view range, energy, food


and weapon strength. In both cases the network should be able to distinguish units differing
in important parameters.

### 7.4 Evaluation of areas and the evaluation cycle

At the very beginning the idea arose of obtaining the global state of the game from the point of view of the computer
player using a neural network having map fields as inputs. At the same time it was clear that
a network with so many inputs would be very hard to train. The final form looks like this: the
map is divided into several areas (more precisely 98), characteristics of the balance of forces
are computed on them, from these the current state of the area is computed using a neural network, from all
these states and global statistics (the same as those for an area, but for the whole map together)
the global state is computed, and from the global state and again the local statistics, for each area
both the importance of the given area and the need to build an attack building (e.g. a tower),
an economic building (accepting raw materials) and also the need to create worker units (able to
mine) in the area are computed (4 different neural networks), and according to these five numbers
some action is triggered in the area (described in more detail in the paragraph on the units' own movements).

Figure 7: Example of the area layout for a 50x50 map. Map fields are shown in black, areas
of the first layer in blue and areas of the second layer in red

The areas are created by dividing the map into 7x7 rectangular areas of size (map
width/7) x (map height/7). If we place this grid so that its origin is at the origin of the
map, we get the first layer of areas. If the origin of this grid coincides with the coordinates
(1/2*area width) x (1/2*area height), we get the second layer of areas. The centre of an area of the first
layer thus always falls on the same place as the corner of four areas of the second layer and vice versa.
Every map field thus always belongs to one area of the first and one area of the second layer,
except for a narrow strip at the edge of the map which is not covered by the second layer, so that the number of areas
in both layers is the same.
The division into 7x7 stems from the fact that in the "8x8" case there are too many inputs for the
neural network for the global state, and for "6x6" the areas are unnecessarily large (for typical maps
with dimensions around 200x200). Therefore the described compromise was chosen.


The neural network for determining the state of an area is stored in the file network_for_state and as
inputs it has the local statistics, which are:
•

the sum of weapon strengths of my units in segment zero,

•

the sum of weapon strengths of my units in segment one,

•

the sum of weapon strengths of my units in segment two,

•

the sum of weapon strengths of all foreign units in segment zero,

•

the sum of weapon strengths of all foreign units in segment one,

•

the sum of weapon strengths of all foreign units in segment two,

•

the number of my attack units,

•

the number of my attack units currently being built (not completely accurate; from each
building only the first unit being built is counted),

•

the number of my worker units,

•

the number of my units currently being built,

•

the number of my buildings,

•

the number of my buildings currently being built,

•

the number of foreign attack units,

•

the number of foreign worker units,

•

the number of foreign buildings,

•

my activity,

•

foreign activity,

•

the amount of raw material 1,
•

the amount of raw material 2,

•

the amount of raw material 3,

•

the amount of raw material 4.

The result of this network is a number between 0 and 0.5, where 0 means my complete superiority, 0.25 –
balanced forces and 0.5 complete superiority of the opponent, or opponents.
Another network used is the network for determining the global state of the game; it is stored in the file
network_for_global. This network has as inputs, first, all 98 areas, then the same

statistics as mentioned above, but in the global version, and finally also the amount of food, energy
(both supplied and consumed) and the current amount of the individual raw materials. Together, therefore,
98 +21 + 4 + 4 = 127 inputs. The output of this network is again a number between 0 and 0.5, this time
however indicating what phase the game has reached from the point of view of the computer player. A number close to 0 indicates
the very beginning of the game, when it is still only necessary to build a base, and conversely a number around 0.5 should
already indicate my complete superiority and just finishing off the enemies. With this view it is admittedly
not entirely clear what each number means, but on the other hand it suits the neural
network better, because situations with similar inputs (for example just one more building on my
side) will also be very close in output value.


When the global state is clear, the evaluation cycle returns again to the level of the individual
areas. Using five different neural networks, five different values are computed for each
network. First the need for active or attack buildings in this area is computed. It has
the following inputs:
•

my strength underground in the area,

•

my strength on the ground in the area,

•

my strength in the air,

•

the opponent's strength underground,

•

the opponent's strength on the ground,

•

the opponent's strength in the air,

•

the number of my attack units,

•

the number of my attack units currently being built,

•

the number of my worker units,

•

the number of my worker units currently being built,

•

the number of my buildings,

•

the number of my buildings currently being built,

•

the number of foreign attack units,

•

the number of foreign worker units,

•

the number of foreign buildings,

•

my activity in the area,

•

foreign activity in the area,

•

the global state,

•

the local state in the area.

The output of the network, which is stored in the file network_for_phaze_a_b, is a number between 0
and 0.5, where 0 denotes zero need to build an attack building in this area and 0.5
an unconditional necessity to build such a type of building here.
The network for economic buildings is also very similar. It differs, however, in its inputs, which in its
case are:
•

the number of my buildings,

•

the number of my buildings currently being built,

•

my activity in the area,

•

foreign activity in the area,

•

the amount of sources of the 1st type in the area,

•

the amount of sources of the 2nd type in the area,

•

the amount of sources of the 3rd type in the area,

•

the amount of sources of the 4th type in the area,


•

the amount of supplied energy,

•

the amount of consumed energy,

•

the amount of supplied food,

•

the amount of consumed food,

•

my amount of material of type 1,

•

my amount of material of type 2,

•

my amount of material of type 3,

•

my amount of material of type 4,

•

the global state,

•

the local state in the area.


This network forms the content of the file network_for_phaze_s_b; its output is a number 0-0.5
with a similar meaning as in the previous case.
The third network, and the largest in terms of number of inputs, is the network from the file network_for_phaze_a_u,
which serves to evaluate the need to produce attack units. It computes its result
from the following statistics.
•

my strength underground,

•

my strength on the ground,

•

my strength in the air,

•

foreign strength underground,

•

foreign strength on the ground,

•

foreign strength underground,

•

the number of my attack units,

•

the number of my units currently being built,

•

the number of my worker units,

•

the number of my buildings,

•

the state of the area in the NW direction (in the same layer of areas),

•

the state of the area in the N direction,

•

the state of the area in the NE direction,

•

the state of the area in the W direction,

•

the state of the area in the E direction,

•

the state of the area in the SW direction,

•

the state of the area in the S direction,

•

the state of the area in the SE direction,

•

my activity,

•

foreign activity,


•

my amount of material of type 1,

•

my amount of material of type 2,

•

my amount of material of type 3,

•

my amount of material of type 4,

•

the global state,

•

the local state in the area.


The last of the networks in this group is stored in the file network_for_phaze_w; it has the same
purpose as the previous one, but instead of attack units it relates to worker
units, and it has these values on input:
•

the number of my worker units,

•

the number of my worker units currently being built,

•

the number of my buildings currently being built,

•

the number of foreign attack units,

•

my activity,

•

foreign activity,

•

the amount of sources of type 1 in the area,

•

the amount of sources of type 2 in the area,

•

the amount of sources of type 3 in the area,

•

the amount of sources of type 4 in the area,

•

the amount of supplied energy,

•

the amount of consumed energy,

•

the amount of supplied food,

•

the amount of consumed food,

•

my amount of material of type 1,

•

my amount of material of type 2,

•

my amount of material of type 3,

•

my amount of material of type 4,

•

the global state,

•

the local state in the area,

The last two also have an output between 0 and 0.5, but numbers smaller than 0.25 indicate that I have more
of the corresponding units here than necessary. The value 0.25 denotes "satisfaction" with the number of
units and greater values the need to call in more.
Somewhat apart from these networks is the network for determining the importance of an area, the file
network_for_importance. Its goal is to order the areas in the way their requirements need to be
satisfied. For it, the numbers of units are more important than their exact kinds, and therefore it has
the following inputs:


•

the average of my strengths in the individual segments,

•

the average of foreign strengths in the individual segments,

•

the total number of my units (including those that are still only being built),

•

the total number of foreign units,

•

my activity,

•

foreign activity,

•

the amount of sources of type 1 in the area,

•

the amount of sources of type 2 in the area,

•

the amount of sources of type 3 in the area,

•

the amount of sources of type 4 in the area,

•

my amount of material of type 1,

•

my amount of material of type 2,

•

my amount of material of type 3,

•

my amount of material of type 4,

•

the global state,

•

the local state in the area.

The resulting number is again between 0 and 0.5 and its interpretation is clear: the higher the number, the
more important the area.

### 7.5 The units' own movements

This evaluation cycle runs repeatedly over and over for all computer players
in the current game. The time interval between two evaluations of the units of the same player is
fixed, and the evaluations of the other players are scheduled evenly within that interval.
At the same time, the activity of the individual fields is computed. Each map field has
its own activity counters for each player, to which 0 is regularly added if on
this field there is no unit of the corresponding player, and a non-zero number otherwise.
This number also depends on the action the unit is currently performing, because, for example, attacking is
considered a more important activity than standing.
The whole course of evaluation from the point of view of the computer player looks like this: first
all area values are recomputed (first all statistics, then all local states of the areas,
the global state and finally the needs of the individual kinds of units and the importance of each area),
then all needs are resolved (e.g. buildings that cannot build units because they
lack food, energy or raw materials), followed by satisfying several areas with the greatest
importance, and finally the remaining units that have not yet received any command are gone through
(workers are sent to mine and attack units are placed in defensive positions).


## 8 Graphics
Peter Knut


### 8.1 Introduction

The game Dark Oberon uses OpenGL technology for displaying graphics. Although it is primarily
intended for 3D applications, it also provides broad possibilities for programs based on
two-dimensional graphics. The advantage is hardware support for rendering with transparency (alpha blending), projections and visibility resolution.

### 8.2 Projections

The application uses exclusively orthographic projection both in the menu and in the game, in which the size
of the rendered polygons is independent of the distance from the imaginary camera. In the menu the view
volume is set according to the window size. In the game the same size is always used, independently
of the window. This achieves the effect that at different screen resolutions and with the application
running full screen, the size of units is the same. This internal resolution is
1024x768. When the map is zoomed in (out), the view volume is reduced
(enlarged).

Map

Screen

View volume

### 8.3 Map display

Each map contains three surface levels – segments. Each segment is represented
as a two-dimensional array and consists of square pieces of surface – fragments. Segments are
displayed in isometric view, so fragments appear as rhombuses with a width
twice their height. Together with suitable textures this view creates an impression of
space.
It is possible to switch between displaying one segment and displaying all segments
at once. In this combined mode the segments are rendered successively from the bottom one
to the top one. They can therefore also contain transparent textures through which the lower
segment will be visible. This is used above all in the highest segment representing the sky.
The game uses a so-called warfog to mark both undiscovered places (black opaque
colour) and places that have been discovered but which no unit currently sees (the colour
and intensity can be chosen). The warfog is nothing other than a texture generated continuously according to the


current situation on the map. One texel represents one field on the map. When
rendering the warfog, its texture is stretched over the whole map. This creates a blurring effect
at the transitions between different types of areas.
Four such textures are maintained continuously. Three are for the individual segments, one for the
combined display of all segments.
An interesting situation occurs in the combined display if a lower segment is partially discovered
in places where the higher segment is not discovered. In this case one sees directly into the
lower segment. The higher segment is clipped using the Z-buffer.

Figure 9: The layout of fragments on the map creates
an impression of space.

### 8.4 Figure 8: Combined display of segments together with
the warfog.

Units

Units are rendered together with the segment on which they stand. For each segment, first
the fragments are rendered and then the units. A special case occurs only when displaying all
segments at once. It is assumed that the textures of the middle segment are opaque. Therefore
the units from the bottom segment are rendered twice. Once together with the bottom segment
and then semi-transparently together with the middle segment, before rendering the units
from the middle segment. This ensures that they are always visible.

#### 8.4.1 Sorting units

Units in each segment need to be rendered from back to front so that they naturally
overlap. For this purpose the unit lists for each segment are kept sorted.
Since unit coordinates do not correspond to the display of the map on the screen, the condition for
sorting is rather complicated. The assumption is that units do not overlap with their
bases.
The basic case occurs when the coordinates of unit A lie in the marked area
of unit B according to the figure. Then unit A is in front of unit B.

B
A’’

A’

A


y


Such a condition, however, is not yet sufficient. There are mutual positions of units for which we
could not decide which unit is closer. In such cases the position of the
outermost corners of the units with respect to screen coordinates is taken into account. In the following figure
unit A is in front of unit B.

y

B
A


A unit can additionally have one of the flags set: lying_down or flying_up.
Units with the lying_down flag lie below units without this flag. This
means that when sorting they are preferentially placed towards the back. The flag is used, for example, for the debris
of a destroyed building. Conversely, flying_up means that the units are above the others
and are placed towards the front. It is used mainly by flying projectiles. It is evident that units with one
of the flags can overlap with units without this flag.

#### 8.4.2 Selecting units

Units can be selected with the mouse via their texture. Texture detection works
by rendering units in the background as black silhouettes. The procedure is as follows:
1. a white square is rendered under the mouse cursor,
2. the units standing under the cursor are gone through successively from bottom to top (with respect to the
screen) and their black silhouettes are drawn. Each time it is tested whether the point under the
cursor is still white. If it is not, the unit is selected and testing ends.
The figure shows a vertical strip of the map under the mouse cursor. Units reaching with their
base into this strip come into consideration for testing. The length of the strip is limited.
Cursor

Map

Testing strip

### 8.5 Radar

The radar displays a preview of the uncovered parts of the map together with visible units. For each
segment there is a texture with an overall view of the map. These textures are obtained
automatically before the game starts. Each segment is rendered in the background so that it exactly fits
into a square of size 256x256 pixels. This area is stored as a texture.
So the texture of the whole segment is always rendered into the radar, then the warfog in the same way as on the
map, and finally the visible units in the form of coloured rhombuses. When displaying all
segments at once, only the middle segment is displayed in the radar (units, of course, from
all segments).


## 9 Graphical interface
Peter Knut


### 9.1 Introduction

The graphical user interface (GUI) was originally created as part of the game Dark Oberon; later
it became a separate library independent of the rest of the project. It is an extension of the
GLWF library (An OpenGL Framework) used in the project. Its basis is, on the one hand, working
with animated textures and, on the other, a panel system creating a variable graphical interface.
This system is focused on simplicity and the needs of the game, so it does not provide the full possibilities
we are used to from ordinary applications.
The source code of the library is located in the files glgui.*.

### 9.2 Textures and animations

A texture loaded from a data file is managed by the class TGUI_TEXTURE. All
its properties are stored here, such as dimensions, number of frames, animation length and the like (see the documentation
of the Data Editor). In addition, this class can render a chosen frame with the method DrawFrame().
Instances of these classes are typically arranged in an array.
Control of the animation itself is the responsibility of the class TGUI_ANIMATION, which refers to
a TGUI_TEXTURE texture. Its main methods are:
•
•
•
•

### 9.3 Update() – advances the animation in time,
Draw() – renders the current frame,
Play(), Pause() and Stop() – serve for starting and stopping the animation,
Hide(), Show() and SetVisible() – set the visibility of the animation.

Panel system

The graphical interface itself is formed by a panel system headed by the class TGUI. After
the interface is created, various active or passive components can be added to it, such as
panels, buttons, lists, checkboxes, etc. All components can render themselves without
intervention using the standard colour palette. If needed, it is possible to set
custom colours or transparency for each element. Several of them support user
textures.
Besides the class TGUI, the connection of the graphical interface with the application consists mainly of the so-called callback
functions contained in all components. These allow the application to be informed
about events arising in the given component (e.g. pressing or releasing a mouse button,
pressing a key, rendering or gaining focus).
The hierarchy of the individual classes is shown in the diagram at the end of the document.

#### 9.3.1 The TGUI_BOX class

The base class forming the common ancestor for all interface components. It defines
a rectangular area with properties such as: position, size, border and various types of colours. The position
is measured from the lower-left corner of the screen. The border reduces the area to the so-called client area.


It is further possible to set whether the area is visible, usable or active. In addition,
a quick help (a so-called tooltip) can be assigned to the area, which is automatically displayed when the
mouse cursor is held over the area.
Most callback functions can be set here. TGUI_BOX contains an optional integer
key, by which the object can be identified in callback functions.

border

client area

height

width

#### 9.3.2 The TGUI_LABEL class

Serves for rendering a given text or animation. The text can
also be multi-line. In that case it is necessary to specify a string
in which the individual lines are separated by the character ’\n’. A useful
property is the automatic resizing of the component according to
its content.

#### 9.3.3 Figure 10: TGUI_LABEL

The TGUI_LIST class

This class is derived from TGUI_LABEL, to which it adds
the ability to select individual lines of text with the mouse and to access
the selected line. It also contains a callback function for a change of
the selected line.
Figure 11: TGUI_LIST

#### 9.3.4 The TGUI_BUTTON class

Represents a button with a caption. The button can be
in various states (pressed, not pressed, pressed active,
not pressed active). For each of these states it is possible to define
a custom texture.
There are three types of buttons, which differ from each other in behaviour:

Figure 12:
TGUI BUTTON

•

ordinary button – after being pressed it returns to its original position (state),

•

check button – after being pressed it remains in the pressed position. It returns to the original position
when pressed again.

•

group button – works similarly to a check button. In addition it cooperates
with the other group buttons that are assigned to the same group. In one
group at most one button can be pressed. So after a group
button is pressed, the currently pressed button in its group automatically returns to its original
position.


It contains a callback function for checking.

#### 9.3.5 The TGUI_CHECKBOX class

This is a specialized class that works exactly
like a check button. The only difference is in the
appearance of the component.
Figure 13: TGUI_CHECK_BOX

#### 9.3.6 The TGUI_EDIT_BOX class

Defines a field into which the user can type
single-line text. It is possible to specify the maximum length
of the text. In the active field a cursor marking the
current position is rendered. At that moment the component receives
keyboard input. The cursor can be moved using the standard
keys.

Figure 14: TGUI_EDIT_BOX

It contains a callback function for a text change.

#### 9.3.7 The TGUI_SLIDER class

Represents a horizontal or vertical slider.
The end positions of the slider are always 0 and 1.

Figure 15: TGUI_SLIDER

It contains a callback function for a position change.

#### 9.3.8 The TGUI_PANEL class

A panel is a visualized rectangular area into which
arbitrary other components (except
TGUI) can be added. The positions of all components in the panel are
relative to the lower-left corner of the client area of the
panel. In addition, components are clipped so that they do not
protrude.

Figure 16: TGUI_PANEL

The panel supports user textures.

#### 9.3.9 The TGUI_SCROLL_BOX class

A scroll box is a panel that automatically manages
built-in scrollbars.
If any of the components
protrudes outside the client area, it is possible to scroll
all
components
and display
the protruding
component.

Figure 17: TGUI_SCROLL_BOX

The scrollbars can be shown or hidden as needed.

#### 9.3.10 The TGUI_LIST_BOX class
This class displays a list of text items,
of which one can be selected. It contains built-in
scrollbars similarly to TGUI_SCROLL_BOX. For its


Figure 18: TGUI_LIST_BOX


work it uses the class TGUI_LIST.
It contains a callback function for an item change.

#### 9.3.11 The TGUI_COMBO_BOX class
The combo

box

works

similarly

to

the class

TGUI_LIST_BOX, which it uses for its work.

However, it does not display all items but only the currently
selected one. At the edge there is a button which, when
pressed, displays the whole list.

Figure 19: TGUI_COMBO_BOX

Figure 20: TGUI_MESSAGE_BOX

#### 9.3.12 The TGUI_MESSAGE_BOX class
This is

a specific

class,

which

is based

on the class

TGUI_PANEL. It can display a given text in the form of

a dialog. It contains several built-in buttons (OK, Yes,
No, Cancel), which can be combined with each other.
The size of the dialog is computed automatically according to the size
of the buttons used and the length of the text. The position of the dialog is always
in the middle of the screen.

#### 9.3.13 The TGUI class
TGUI is the base class for working with the graphical

user interface. The existence of exactly
one instance of this class is assumed. It is actually an invisible
panel stretched over the whole screen. The basic
functions are:
•
•
•
•
•
•

Update() – this function must be called

always before rendering. The parameter is the time
shift since the last call of this function,
Draw() – renders all visible components,
MouseMove() – must be called on every
change of mouse position,
MouseDown() – must be called after every
press of a mouse button,
MouseUp() – must be called after every
release of a mouse button,
KeyDown() – must be called after every
key press.


Figure 21: Example of the graphical
interface using user
textures and colours.


The class also allows setting the font type and colour globally for all components
and displaying a dialog with a message (using TGUI_MESSAGE_BOX).
A detailed description of all classes and functions can be found in the source code of the program.
Class hierarchy of the graphical interface:
TGUI BOX
TGUI_LABEL

TGUI_LIST

TGUI_BUTTON

TGUI_CHECK_BOX

TGUI_SLIDER
TGUI_EDIT_BOX
TGUI_LIST_BOX
TGUI_COMBO_BOX
TGUI_PANEL
TGUI SCROLL BOX
TGUI_MASSAGE_BOX
TGUI


## 10 Network
Marián Černý


### 10.1 Introduction
The Dark Oberon project uses the TCP/IP protocol with a peer-to-peer architecture for network communication, where each computer has connections established with all computers that
participate in the game. Starting the game is somewhat different, when a client-server architecture is used.
The computer that creates the game is called the leader. The other computers that connect to it are
called followers.
Communication takes place in the form of messages, which have their header and their own content (data).
Each message has a recipient. The recipient is a numeric player identifier, or a special
value meaning all players. A message intended for all players is delivered to each computer
only once, even if several players run at once on some computer (the leader runs the
hyper player, the human player and possibly also computer players).
All classes and functions responsible for network communication are implemented in
the files donet.h, dohost.h, dofollower.h, doleader.h and the corresponding .cpp
files.

### 10.2 Basic classes for working with the network
The basic classes for working with the network are implemented in the files donet.h and donet.cpp.
These classes include:
•

TNET_MESSAGE – representing a message sent to or received from the network,

•

TNET_MESSAGE_QUEUE – a message queue into which messages can be inserted and from which

messages can be taken,
•

TNET_LISTENER - a class waiting for incoming connections and inserting network

messages into the queue of incoming messages,
•

TNET_TALKER - a class that sends network messages from the queue of outgoing messages

to the individual recipients,
•

TNET_DISPATCHER - the deliverer of incoming messages.

#### 10.2.1 The TNET_MESSAGE class
The class TNET_MESSAGE represents a network message. A network message is a sequence of bytes
consisting of a header and a message body (data). The message header contains the message size, the message
type, the message subtype and the message recipient. Then the data itself follows.
message

message

message

message

message body

size

type

subtype

recipient

(data)


#### 10.2.2 The TNET_MESSAGE_QUEUE class
Network messages are stored in a queue before sending and upon receipt from the network. This queue is
represented by an object of the class TNET_MESSAGE_QUEUE. The queue has a certain size, which is
given as a constructor parameter.
The class TNET_MESSAGE_QUEUE provides two basic functions:
•

PutMessage() - inserting a message into the queue,

•

GetMessage() - taking a message from the queue.

Both functions can be blocking. The function PutMessage() blocks when inserting a message
into a full queue and the function GetMessage() when taking a message from an empty queue. This
blocking is ensured by condition variables, and the whole class is safe for use by
multiple threads.
The queue is implemented as a circular array with a head.

#### 10.2.3 The TNET_LISTENER and TNET_DISPATCHER classes
TNET_LISTENER is a class that represents a receiver of network messages. The class contains

a queue of incoming messages. An object of this class creates a new thread in its constructor, which
waits for incoming TCP connections. For each new incoming connection a
new thread is created, which receives messages from the network and inserts them into the queue.
TNET_DISPATCHER is a class whose task is to deliver received messages. It contains a pointer

to a message queue, from which it takes the individual messages and, according to their type, executes the corresponding
registered functions on them. This task is performed by the auxiliary class TNET_HANDLER. All
activity of the class TNET_DISPATCHER runs autonomously in a separate thread, which is created
in the constructor.

#### 10.2.4 The TNET_TALKER class
Sending network messages is the responsibility of the class TNET_TALKER. The class contains a queue
of messages to be sent. The object instance creates a new thread in its constructor, which in an
infinite loop takes new messages from the queue of messages to be sent and sends them over the network
to the given recipients. If the recipient is the special value, the message is delivered to all
recipients, but only once to each address. Recipient addresses are specified using the function
AddAddress(). When an address is added, a TCP connection is created automatically.

### 10.3 Network interface - the THOST class
The class THOST represents a simple network interface containing everything needed for
network communication. It integrates all the basic network classes into one. Its structure is
shown in figure no. 2:
Besides the basic function for sending messages SendMessage() and the function for creating
a network
connection
AddRemoteAddress(),
the class
also
contains
the functions
RegisterSimpleFunction() and RegisterExtendedFunction(), by which it is
possible to determine which function will be called upon receipt of the individual message types.
The host class is implemented in the files dohost.h and dohost.cpp.


THOST
thread
TNET_TALKER
TNET_MESSAGE_QUEUE
incoming messages

thread
thread

TNET_LISTENER

thread

thread
TNET_HANDLER
TNET_MESSAGE_QUEUE
outgoing messages

thread

TNET_DISPATCHER

Figure no. 2: The THOST class

### 10.4 Starting the game
As already mentioned above, a client-server architecture is used when starting the game. The computer
that starts the game is called the leader. Its task is to accept connections from the other computers and
pass them information about the currently connected computers - IP addresses, player names and chosen
races. The leader alone selects the map that will be played, and the hyper player also runs on it - the player
who owns the sources. It is also the only one that can add computer players. The leader uses
as its network interface an object of the class TLEADER, which is a descendant of the class THOST. Compared to
THOST it additionally has functions that send network messages specific to this type of
computer (see Message types below).
Connecting another computer is done by creating a TCP connection to the port on which the
leader listens and sending the network message net_protocol_connect, which contains information about
the connecting computer: the port on which it will accept connections from the other followers, and the name
of the connecting player. The leader adds it to the array of connected players and answers with the message
net_protocol_hello containing the IP address of the connecting follower and the current time. The IP
address is needed so that the follower can determine which player in the player array is its own. To
the received current time, half of the time it took for the answer from the leader to arrive is added.
This performs the first rough time synchronization. Further time synchronizations are performed by
repeatedly sending the network message net_protocol_ping. Whenever an answer is
faster than any of the previous answers, the time is adjusted according to that answer.
On every change of information about the connected players, the leader sends to all connected
computers the network message net_protocol_player_array containing information about


all players including the player's name, the chosen race and further supplementary information, such as
whether it is a computer player or a player running on the leader, and the starting field
(start_point) where the given player will start on the map. In addition, information about the
currently selected map is sent. If a player on a follower changes race, it sends the leader
the message net_protocol_change_race with the name of the new race.
LEADER

FOLOWER1

FOLOWER3

FOLOWER2

The game is started by clicking the Play button on the leader. Then the final form of the array
of connected players is sent in the network message net_protocol_player_array, this time however with
subtype 1, which informs all computers that the game is to be started on the selected map with the given
players. To create the peer-to-peer architecture it may be necessary to create further connections
between the connected followers. The connection is always created by the follower that has the player with the lower
number, to the follower that has the player with the higher number.
LEADER

FOLOWER1
1.

FOLOWER3

2.

1.

FOLOWER2

### 10.5 Message types
The file dohost.h declares the individual types of network messages that can be
sent over the network. The most important message type is the message net_protocol_event, which
contains a TEVENT structure describing some action event that needs to be scheduled in the
queue of the remote computer. These messages describe all events that can occur during the game
- walking, attacking, mining, building and repairing, producing units and regeneration
of sources. Therefore the other message types are only messages needed to synchronize players during
connection in the menu and possible further events:
•

Common messages:
o net_protocol_chat_message – a message with text that is displayed to the other
players,
o net_protocol_synchronise – a message informing the others that the given player
is ready to start the game.

•

Messages sent by the leader:
o net_protocol_player_array – a message containing information about all
players and the name of the selected map,


o net_protocol_hello – a message sent as a reply to
net_protocol_connect.
•

Messages sent by a follower:
o net_protocol_connect – a message sent upon connecting,
o net_protocol_change_race – change of the given player's race,
o net_protocol_ping – determining the current time.


## 11 Configuration files
Peter Knut


### 11.1 Introduction
Configuration files arose already at the beginning of the project for setting the basic properties
of the application such as window size, mouse sensitivity, etc. Gradually, as the requirements on
these files changed, their form also changed from a simple line-based one to a structured one. In this way
a universal system for working with configuration files arose, which is used both for
setting application properties and for the definitions of individual maps, schemes and races. They provide
high variability while keeping the notation simple. Thanks to the textual form, the user can
edit these files without using a special editor.

### 11.2 File structure
The files recognize four basic components: items, sections, comments and empty lines.
These components can be combined with each other while observing certain rules.

#### 11.2.1 Items
Each item is on a separate line of the file. Items have the form:
"item name" ["value" ["value" …]]
The item name is an arbitrary string of characters. If the name contains no whitespace characters
(spaces or tabs), quotation marks are not necessary.
The name may be followed by a varying number of values. Again, if a value contains no
whitespace characters, quotation marks are not necessary. The individual values are separated from each other and from the name
by whitespace characters. Configuration files support several types of values. In addition,
the user may be required to enter some values within a bounded interval.
Value types:
•
•
•
•
•
•

string – an arbitrary string of characters,
byte – an integer in the interval <0, 255>,
integer – an integer in the interval <–2 147 483 648, 2 147 483 647>,
bool – allowed values are: true, false, yes, no, 0, 1,
float – a real number in the range 3.4E +/- 38,
double – a real number in the range 1.7E +/- 308.

The individual items must have mutually different names within a section. If this is not
observed, only the first item will be accessible.
Examples of items:
fullscreen false
show_fps yes
resolution "800x600"
"warfog color" 100 255 50
sensitivity 0.6


#### 11.2.2 Sections
Sections serve to group semantically related components of the file. They have the form:
<"section name">
</[character string]>

The section name is an arbitrary sequence of characters. Quotation marks are not mandatory. The character string
is ignored during processing. However, it is advisable to use it to make the file clearer. The beginning
and end of a section must be given on a separate line. A section can contain arbitrary
file components including further sections. Section names at the same level must not be identical,
otherwise only the first of them will be accessible.
Example of using sections:
<players>
max_count 2
<player 0>
race "humans"
position 10 125
</player>
<player 1>
race "orgs"
position 200 53
</player>
</players>

#### 11.2.3 Comments and empty lines
To improve the clarity of the notation it is good to use empty lines and line indentation.
The following example shows typical use of all components:
# Player definitions
<players>
# Maximum number of players
max_count 2
# Each player contains a race and a starting position.
# Positions are in the range from 0 to 255.
<player 0>
race "humans"
position 10 125
</player>
<player 1>
race "orgs"
position 200 53
</player>
</players>


### 11.3 Classes and methods
For working with configuration files, the classes and methods from the module dofile.* are used. The file
itself is handled by the class TCONF_FILE. When creating an instance of this class it is enough to give the path
to the file and then load its content with the method Reload(). The whole content is loaded into
a tree structure formed by instances of the classes TFE_SECTION, TFE_ITEM and TFE_LINE.
The class TCONF_FILE contains a set of functions for moving among sections and of course functions for
reading and writing items to the file. A section is entered by calling the function
SelectSecion()
with the name
of the section
as
a parameter
and left
by calling
UnselectSection(). This means that a section located at a deeper level is accessed by
calling SelectSection() multiple times – successively with the names of the individual sections.
From the current section it is then possible to read the values of the individual items with the functions
Read<type>() and write them with the functions Write<type>(), where type is:
•
•
•
•
•
•
•

Str – for items of type string,
Byte – for items of type byte,
Int – for items of type integer,
Bool – for items of type bool,
Float – for items of type float,
Double – for items of type double,
Simple – this version is identical to Byte. It is always used in connection with dimensions and

positions on the map.
For numeric types there are additionally the functions: Read<type>GE() – for specifying the lower bound
of the value being read, and Read<type>Range() – for specifying the lower and upper bound
of the value being read.
When the reading or writing functions are called repeatedly on the same item, the individual values are successively read
/ written. All reading functions have, as one
of the parameters, a default value which is used as the result of reading in case
an error occurs (for example when the item does not exist).
For simpler opening and closing of configuration files there are global functions:
CreateConfFile(), OpenConfFile() and CloseConfFile().
A detailed description of the classes and functions can be found in the source code.


## 12 Logging
Peter Knut


### 12.1 Introduction
Writing to logs is used both for printing possible errors when loading configuration
files and, last but not least, for debug output. All records are stored in the file
logs/full.log; error records are additionally written to the file logs/error.log. Records are
also printed to standard output. It is also possible to register a custom callback function for
processing records. This is used e.g. for printing logs on the screen.

### 12.2 Record types
Logs can be of various types according to their meaning. The following macros are defined for the individual
types:
•
•
•
•
•

Info() – information about the action being performed.
Warning() – a warning about an insignificant error that does not cause the action to stop.
Error() – an error in performing an action. The action is cancelled, but the application keeps running.
Critical() – a critical error that causes the whole application to terminate.
Debug() – debug output. When the Release version of the program is compiled, these outputs are

ignored.

### 12.3 Record format
The format of the records differs according to the system on which the program is compiled and also according to
whether it is a Debug or Release version.
Records are generally of the following form:
[file_name:line_number] <header> <record text>

The first column contains in square brackets the name of the source file and the line from which the
record was written. This column is printed only in the Debug configuration. The header differs according to the
system. The following table gives an overview:

Record type (macro)

Windows

Unix and others

Info()

[Info]

Info:

Warning()

[Warning]

Warn:

Error()

[Error]

Err:

Critical()

[Critical]

Crit:

Debug()

[Debug]

DBG:

A detailed description of the individual macros and functions can be found in the source code of the program
(files dologs.*).


## 13 Data Editor
Peter Knut


### 13.1 Introduction
The Data Editor program allows creating and editing the data files used in the game Dark
Oberon. A data file contains two basic types of records: textures (image data)
and sounds.
The application is programmed in the Borland C++ Builder 5.0 environment for the
MS Windows operating system.

### 13.2 Source files
The application contains four basic modules divided into source files in the src directory:
•
•
•
•

datedit.*
–
demainfrm.*
–
deaboutfrm.* –
dedatafile.*
–
and its records.

the main module of the application with automatically generated code,
the main window,
the About window,
contain the data structures and methods for working with the data file

### 13.3 Main window
The main window of the application (MainForm) contains a menu (MainMenu), a toolbar (ToolBar),
a tree diagram showing the structure of the data file (TreeView), a splitter for
dynamically resizing the diagram (Splitter), and three panels with controls for
the individual record types of the data file. When the user selects some
item in the diagram, the panel corresponding to this item is displayed and its controls are filled with
the current values. The other panels remain hidden.
The window also contains several hidden components. The icons for the menu items and the toolbar
are stored in the ImageList component. For opening and saving various files the
standard dialogs FileOpenDialog, FileSaveDialog, TextureOpenDialog,
TextureSaveDialog,
SoundOpenDialog,
SoundSaveDialog,
ImportDialog
and ExportDialog are used.

### 13.4 About window
The About window (AboutForm) displays the icon,
name, version of the program and the licence terms. It contains
a button for closing the window (OKButton).

### 13.5 Editing data
Working with the data file is handled by the class TDataFile.
It contains two doubly linked lists: a list
of texture groups and a list of sound records. A texture
group is represented by the structure TTextureGroup. It
contains a linked list of texture records


Figure 22: Data structure


(TDataTexture). A sound record is represented by the structure TDataSound.
A detailed description of the individual variables and methods can be found in the source code of the program.

### 13.6 Data file format
Data files are binary and have the extension dat. The current version of the data file is 3.
In the file layout, for each item the type of the variable used for writing
and reading the given item is given.
File layout:
•
•
•
•
•

•

header – always the string "Dark Oberon data file" – 21 x char,
file version – unsigned char,
start of the file block with texture groups – long. If there are no texture groups
in the file, this start is set to zero,
start of the file block with sounds – long. If there are no sound records
in the file, this start is set to zero,
file block containing textures. This block is present in the file only if
at least one texture group exists in it:
o number of texture groups – int,
o the individual groups:
length of the texture group name – unsigned char,
string with the group name – n x char,
number of textures in the group – int,
texture records:
• length of the texture name – unsigned char,
• string with the name – n x char,
• horizontal number of frames in the animated texture – unsigned
char. In the interval <0, 100>,
• vertical number of frames in the animated texture – unsigned
char. In the interval <0, 100>,
• animation length in milliseconds – int. In the interval <0, 10000>,
• x position of the origin of the coordinate system – int. In the interval
<-1024, 1024>,
• y position of the origin of the coordinate system – int. In the interval
<-1024, 1024>,
• texture type – unsigned char. Allowed values are given
in the table below,
• size of the source data – unsigned int. Can also be zero,
• source data – n x unsigned char. Only if the size
of the data is greater than zero,
file block containing sounds. This block is present in the file only if
at least one sound record exists in it:
o number of sounds – int,
o the individual sound records:
length of the sound name – unsigned char,
string with the sound name – n x char,
sound format – char. Allowed values are given in the table below,


sound type – char. Allowed values are given in the table below,
size of the source data – unsigned int. Can also be zero,
source data – n x unsigned char. Only if the size of the data is
greater than zero.

Texture types:

Ordinary square texture
Texture intended for a terrain fragment

Sound types:

Sample
Stream

Sound formats:

WAV
MP2
MP3
OGG
RAW
MOD
S3M
IT
RMI
SGT


## 14 Map Editor
Peter Knut


### 14.1 Introduction
The Map Editor program allows creating and editing the configuration files of maps used
in the game Dark Oberon. This editor is not complete; it is focused only on editing the surface of maps.
The application is programmed in the Borland C++ Builder 5.0 environment for the
MS Windows operating system.

### 14.2 Source files
The application contains several basic modules divided into source files
in the src directory:
•
•
•
•
•
•
•
•
•

mapedit.*
– the basic, automatically generated code of the application,
memain.*
– the code of the main window,
meabout.*
– the About window,
menewmap.* – the dialog with the map properties,
mefragsize.*
– the dialog with the fragment size,
mefragments.* – the dialog for defining the colour scheme,
memap.*
– contain the class for working with the map file,
mescheme.*
– contain the class for working with the colour scheme file,
mefile.*
– contain universal data structures and methods for working
with a configuration file. These files are taken from the source files of the game Dark
Oberon and adapted for use in the Map Editor.

### 14.3 Main window
The basic parts of the main window of the application (frm_main) are: a menu (mnu_main), a table
of fragments and the colour scheme (grd_scheme), a field for selecting the current segment
(rg_segment), a splitter for dynamically resizing the table (splitter), and an image
for rendering the map (img_map). The size of the image changes automatically based on the
size of the opened map so that the map field representing one fragment is always
the same size.
The window also contains several hidden components. The icons for menu items are stored
in the component il_menu. For opening and saving files the standard dialogs
glg_open and dlg_save are used.

### 14.4 Auxiliary dialogs
The dialog frm_new_map serves both for the initial setting of the map properties when creating a new
map and then for changing its properties during editing. The dialog includes fields for
setting the map dimensions and the fragment size; and buttons for confirming or cancelling
the settings.
When opening an existing map it is necessary to enter the fragment size the map uses. For
this purpose the dialog frm_frag_size was created.


Using the dialog frm_fragments it is possible to edit the colour scheme. The dialog contains
a field for choosing the segment (rg_segment), edit fields for the fragment number and name,
an image for displaying the fragment colour (pic_color) and finally a button for closing the
dialog (btn_done).

### 14.5 About window
The About window (frm_about) displays the icon, name, version of the program and the licence
terms. It contains a button for closing the window (btn_ok).

### 14.6 Data structures
Working with the configuration files of the map and the colour scheme is handled by the classes TMAP_FILE
(memap.*) and TSCHEME_FILE (mescheme.*) using the class TCONF_FILE (mefile.*).
The colour scheme is kept in the global variable frg_info of type TFRAGMENTS_INFO,
the map in the array map_arr (memain.*).
A detailed description of the individual variables and methods can be found in the source code of the program.

### 14.7 File formats
The format of the map configuration file is given in the documentation on creating a custom map
in the game Dark Oberon.
Files with colour scheme definitions are textual and have the extension col. They use the same
formatting system as the other configuration files of the game Dark Oberon. They contain three basic
tags <Segment #> for each segment, where # is the segment number (0, 1 or 2). The tag
<Segment> contains, first, the item count with the number of fragments in the corresponding segment,
and then items of the form:
fragment_# r g b name

where # is the fragment number starting from zero; r, g, b are numbers from the interval <0, 255> defining the colour
of the fragment by component (red, green, blue); name is the name of the fragment.
Example of a colour scheme:
<Segment 0>
count 1
fragment_0 108 221 114 "clay"
</Segment 0>
<Segment 1>
count 3
fragment_0 108 221 114 "grass"
fragment_1 192 192 192 "rocks"
fragment_2 72 196 255 "water"
</Segment 0>
<Segment 1>
count 0
</Segment 0>
