# Project Documentation

*Translated from the original Slovak PDF `Project_documentation_SK.pdf` (pdftotext + heading structure). Images and exact formatting from the PDF are not included.*

---

Valéria Šventová
Martin Košalko

### 1.1 Introduction
The Dark Oberon project was developed as part of the course "PRG023 – Project" at MFF UK
under the supervision of RNDr. Jakub Yaghob.

### 1.2 Brief project description
The goal of the Dark Oberon project was to program a strategy game similar in character
to games such as Warcraft or C&C Red Alert. As is usual in such games, each player can
produce various types of units and buildings, send formations of their units against the
enemy, and so on. The winner is the player who destroys all of the opponent's
units.
Dark Oberon has only a multiplayer mode with communication over a local network. An
experimental computer player was also implemented as part of the game.
The OpenGL interface was used for graphical output.
DarkOberon gives the user broad possibilities for creating an entirely new game of this type
by means of configuration files, in which many properties of the units, or of the game
itself, can be defined.
The whole game is portable to the Windows and Unix (X11) platforms.

### 1.3 Development team
From the very beginning the project team consisted of six members. This number did not
change during development. What follows is an overview of the team members with a
description of their work on the project:
Marián Černý (jojo@matfyz.cz)
Design and implementation of network communication, portability to
Unix, CVS administration, project web pages, photographing
models, documentation.

Peter Knut (peter.knut@matfyz.cz)
Design and implementation of the game's graphical interface, the Data and Map editors,
web page design, Human race models, texture processing, design
and implementation of data files, design of configuration files,
documentation.


Martin Košalko (cauchy@matfyz.cz)
Design and implementation of the messaging system for the individual game units,
the message queue, design of the interpretation of neural network results, models
of the Plastic scheme, Map editor, implementation of configuration files,
documentation.

Michal Král (index@matfyz.cz)
Design and implementation of the intelligent computer player, creation of
neural networks, documentation.

Jiří Krejsa (crazych@matfyz.cz)
Design and implementation of the shooting and walking algorithms, thread pool,
documentation.

Valéria Šventová (liberty@matfyz.cz)
Design and implementation of algorithms for unit actions, testing,
documentation.

The division of work was not precisely defined at first; only the main directions of
development were assigned (graphics and user interface (1), network support and portability (1),
algorithms directly concerning the game itself (2-3), computer player (1-2)). Over time
new tasks appeared and the existing ones took clearer shape.

### 1.4 Development
#### 1.4.1 Development tools
We chose C++ as the programming language and made extensive use of its object-oriented
features in the project. Graphical output is implemented using the OpenGL interface
(www.opengl.org).
Portability of the application is ensured by the GLFW library
(glfw.sourceforge.net). The FMOD library (www.fmod.cz) is used for sound output.
The project's source files are stored on the CVS server of the SourceForge.NET service
(www.sourceforge.net).

#### 1.4.2 Development chronology
The chronological description of the course of the work is approximate; many of the activities
mentioned extended beyond the stated time range. Initially the intention was to create
just a single game within the software project; later we moved on to creating a universal engine for games.
The requirements were maximal: generality, as few limitations as possible, and configurability. Over time,
however, it turned out that too much generality brings considerable problems, for example
a demanding implementation of many algorithms. These requirements were therefore often
dropped. The following description of the development of the DarkOberon project also covers the major
changes the project went through.
•

10 October 2002 – first informational meeting of the project members (from which the
idea of creating a strategy game came), choice of language, first division of tasks,

•

end of October 2002 – official announcement of the project by the committee,

•

November 2002 – January 2003 – design of the internal concept, decisions on
the individual implementation steps of the basic parts of the game (maps, computer players,
management of players' units, properties of individual unit actions (walking, shooting,
creating new units, mining materials, etc.)), registration of the project on
SourceForge and first use of CVS, creation of the first files,

•

February – June 2003 – implementation of the game's internal structures, the walking algorithm,
basic algorithms for the individual unit activities, and the game's graphical interface.
The project underwent its first significant change: originally the intention was to create multiple
segments (levels in which units can move), whose number would be
specified through configuration files. Because the implementation was too difficult,
we switched to implementing exactly three segments
representing the underground, the ground surface and the air. In addition, it is assumed that the topmost
segment (air) contains semi-transparent or transparent textures, while the remaining segments
can use arbitrary textures,

•

October 2003 – January 2004 – at the beginning of this period the project underwent another
change: the idea of general dependencies receded into the background and was replaced by dependency
on ancestors and on the amount of materials. With a general dependency it is assumed that the
properties of a unit (building) depend on the existence of other units (buildings).
Dependency on ancestors only expects the existence of the unit itself on which the
upgrade will be built.
This period is also characterized by the beginnings of the design of the intelligent computer player.
A meeting took place with Mgr. Roman Neruda, where the possibilities
and pitfalls of implementing an intelligent computer player using multilayer
perceptron networks were discussed. The basic algorithms for the individual units were also being completed.

•

February – June 2004 – after a thorough analysis we changed the game's architecture: we moved from Update
functions to a message queue. The Update function was called for every unit in a loop
until the game ended, and all actions associated with that unit took place in it. The message
queue has a different character – messages from all units are inserted into it; each
message has a timestamp and represents some action of a unit. It also contains
information about whom it is intended for. Messages are taken from the queue according to their timestamp and


processed. A more detailed description of how the message queue works is given in separate
documentation.
The period from April is characterized by the implementation of neural networks (computer player),
a renewal of the graphical interface – new textures were created by photographing unit models that
were made of plasticine; testing of the already programmed parts,
•

October – December 2004 – introduction of multithreading (the "main"
thread runs user input and graphical output, the other threads process
e.g. messages, walking, distance calculations, etc.), study of networking issues,
design and implementation, testing of the programmed neural networks and first
attempts at interpreting their results,

•

January – May 2005 – testing, documentation, programming of network support
continues, fine-tuning of details.

Throughout the whole duration of the project, project meetings took place almost regularly,
with the exception of the summer holidays. The frequency of meetings was influenced by the intensity of the project's
development. In the second year of development, meetings took place regularly once a week in the
evening. In addition, a meeting with the project supervisor took place once every two weeks.
Towards the end of the project the frequency of the latter meetings increased to once a week.

#### 1.4.3 Original project intentions and the result – a comparison
##### 1.4.3.1 A specific game versus a game engine

The development team's original intention was to create a Warcraft 2-style strategy game that
would be modifiable through input data. According to the specification, it was to be possible to change
the properties of specific units (the airship could fly faster, the warrior could get
more strength...). It turned out that having only a limited, predefined number of unit types
could be restrictive for the user, so we made it possible to define any number of unit
types with user-defined properties. This, however, placed higher demands on
programming, because almost nothing can be assumed about the unit types the user defines.
We thus drifted toward programming a general engine for strategy
games. Some algorithms, however, we were unable (or it was not desirable and effective)
to generalize completely, so we cut back on the requirements. An example of an algorithm that was not
suitable to implement generally is the graphical display of an unlimited number of segments
(so we settled on a fixed number of segments). The result is therefore an almost general template –
an engine for real-time strategy games (so-called RTS).

##### 1.4.3.2 Intelligent computer player

In the early stages of the project we wanted to implement an intelligent computer player
that would be able to "learn by observation" the game strategy of its opponents. The results of every
game were to be saved, then processed and evaluated. Based on the results achieved,
the neural networks deciding the computer player's strategy were then to be modified, thereby
ensuring its development.
The first problem turned out to be the evaluation of a played game, which is quite non-trivial. It is
namely difficult to decide whether a given action performed by the player (or by a neural network)
at some moment of the game was ultimately positive, and to find (and quantify) the degree of positivity of that
action. The idea of saving and evaluating games was therefore abandoned and is not implemented.
Already at the meeting with Mgr. Roman Neruda we were warned that the networks that
came into consideration for deciding the player's activity could be, given the input


parameters (their number) and the number of learned examples, non-trivially large. The non-trivial
size of the networks immediately implies a considerable training time as well. In practice it turned out
that in order to teach the network all the required cases, a truly
huge network is necessary (which was not yet that big a problem) as well as a large number of sample examples that
overlap each other. The biggest problem was finding a scoring of the sample examples
(needed for the back propagation algorithm) such that the network would be
able to learn them.
The result of our efforts is therefore an implementation, or rather an attempt at implementing,
an intelligent computer player using a system of multilayer neural networks.
Some player actions we managed to handle better (mining materials, building combat
units and the necessary buildings), others less so (attacking). Our original ideas were, however,
greatly overestimated and we were not able to fulfil them completely. By its
intricacy and complexity, the implementation of the intelligence is probably worthy of a software
project of its own, which could be based on the already existing game.

##### 1.4.3.3 Graphics

The graphical side of the project met our original intentions. The OpenGL interface worked reliably
on all tested systems. In accordance with the specification, the project supports
multilayer graphics. This is visible in full glory in the "multiview" view of the map, where
units from the lower segment are displayed semi-transparently (using the alpha channel).
All environment elements and players' units are displayed as 2D images – textures. To
create a sample game we therefore needed a large number of textures (images of buildings,
little figures, trees). The first idea was to create 3D models in 3D Studio
Max and then convert them into the required format. This proved to be inefficient and time
consuming. We therefore looked for a less time-consuming method, which could be photographing
real models. We went through many ideas (Lego, "igraček" toys, real human
figures...) until the idea came to model the whole environment out of plasticine. We (also
thanks to external "project members" and the patient work of our graphic artist) fully realized this idea.
The models had to be sculpted (which was a pleasant diversion from the work), photographed
from all sides for every activity, and the photographs then processed into the required
format. The result is thus a plastic world that is original and pleasing to the eye.

Figure 1: Photographing the piglet


Given the large number of textures needed for the animations of all unit
activities, not all units are fully animated. We did, however, try to make it possible
to show every action on at least one unit.

##### 1.4.3.4 Portability

In the project specification we stated that the program should be portable to the
Windows and Unix/X11 platforms. It seemed to us that this type of program was lacking on Unix
systems. We managed to fully achieve this goal thanks to the libraries used, which
handle input and output (GLFW, FMOD), and also thanks to the use of standard C/C++
functions.
The program was developed on the Win32 platform (in MS Visual Studio 6.0, later MS Visual
Studio .NET 2003), and on the Unix platforms FreeBSD and Mandrake Linux (vim, gcc). The program works on
these platforms (FreeBSD without sound). The program should also be portable
to all platforms listed in the GLFW and FMOD specifications.

##### 1.4.3.5 Networking

When creating the project specification we only agreed that the game should have a multiplayer
mode and should run over a local network. This intention was achieved, which we can be
satisfied with.
During the development of the project, however, several excellent ideas concerning networking came up, but
we did not manage to implement them. We wanted to connect the computers in the network (dynamically) so that
the load on the computers and links would be optimal. Related to this is the placement of the computer
players, who were to be distributed among all computers according to computing power –
currently they all run on the game creator's computer. Similarly, there was an intention that after
one of the players disconnects, their role would be taken over by a computer player running on another (still connected)
computer – currently, after a player disconnects, a disconnect message is sent to all remote computers,
whereby the player is deactivated everywhere (their units die properly).
It is fair to say that, given enough time, many things could be improved.
