# User Documentation

*Translated from the original Slovak PDF `User_documentation_SK.pdf` (pdftotext + heading structure). Images and exact formatting from the PDF are not included.*

---

## 1 Minimum requirements
and installation
Marián Černý, Matrin Košalko, Peter Knut


### 1.1 Minimum requirements
To run the game successfully and smoothly, the following minimum computer configuration
is required:
•
•
•
•
•
•

processor – AMD Athlon 1GHz or an equivalent Intel processor
graphics card – with OpenGL 1.1 support, at least GeForce 2 level (64 MB)
memory – 512 MB
CD ROM – for installation
network card – for multiplayer mode
sound card - optional

### 1.2 Installation
#### 1.2.1 Installation on Windows systems

The installation CD contains an installer of Dark Oberon for Windows
systems. Simply run the installation file doberon.msi and, during installation, choose the directory
where the program will be installed (the default directory is C:\Program Files\darkoberon\). The rest happens automatically. The installer creates shortcuts to
all necessary components in the "Start" menu. The program is launched via the shortcut in the "Start" menu, or by running
the file doberon.exe in the directory where the program was installed.

#### 1.2.2 Installation on Unix systems

To build successfully, the project needs the GLFW library version 2.5, sometimes labelled
as 2.5.0. Its standard location is expected to be the file /usr/X11R6/lib/libglfw.a.
If this library is not present on the system, it must be installed. If the user
does not have permission to install into system directories, the file libglfw.a can also be placed
in the directory dark-oberon/libs (after step 1 below).
Overview of the commands needed for installation on Unix systems into the current directory:
1. tar -xzvf /mnt/cdrom/dark-oberon.tgz
2. cd dark-oberon
3. make

This procedure creates the game executable <code>doberon<code>, which is launched
with the command ./doberon.
Description of the individual commands:
1. the command extracts the archive dark-oberon.tgz from the installation CD into the directory darkoberon in the current directory. A valid path to the archive darkoberon.tgz must be given. On some systems the CD-ROM drive is accessible in a
directory other than /mnt/cdrom. Often it is /cdrom and sometimes /media/cdrom.
The CD-ROM drive will probably need to be made accessible with the mount command – e.g.
mount /mnt/cdrom (the user must have the necessary permissions, see mount(8) ),
2. changes the current directory to dark-oberon,


3. Starts the build. By default the project is built without sound support, because the
fmod library we use in the game is not available for all Unix platforms. If the
library is available on the system, the project can be compiled with sound support. You
need to enter the command: mount –DSOUND instead of the plain make command.


## 2 Game manual
Valéria Šventová


### 2.1 General description of the game
Dark Oberon is a strategy game in the style of Warcraft 2 or C&C – Red Alert. Each
player, according to their abilities and possibilities, produces units with various properties during the game
(these properties can be defined in a configuration file), builds buildings that can
receive materials or produce various types of units, attacks the enemy, mines resources, etc.
The goal of the game is to gain superiority over the opponent in the absolute sense, which means destroying
the opponent's units and preventing them from producing more.

### 2.2 Starting the game
The game is started by clicking the file doberon.exe.

### 2.3 Game controls
#### 2.3.1 Main menu

After the game starts, the user is shown the main menu, as depicted in figure 1.

Figure 1: Main menu

The individual items of this menu are described in the following paragraph.
•

Play – leads to creating a new game, redirects the player to another menu in which they
can choose the options:
o Resume – allows returning from the menu to an already running game,
o Create – creates a new game. The player's name can be changed in the menu that
appears immediately after selecting the CREATE option from the main menu (figure
2) and clicking Create (selecting Back returns the player to the
previous menu),


Figure 2: Create menu
H
H
The player can further choose the map type (figure 3) by clicking the
map in the MAP section, and computer players by clicking the button
AddComputer. The players field shows all connected players including
computer ones. Each player must represent a different faction of units (for example
Plastic Humans, Platic Humans – Yellow, etc.), which can be chosen
in the drop-down menu (figure 3).

Figure 3: Players, maps, info

o Disconnect – ends the running game,
o Connect – allows the player to enter a server by IP address or network
name. If an error occurs while connecting, a warning message is shown,
o Back – selecting this option returns the player to the start menu (figure 1),


•

Quick Play – immediately takes the player straight into the game, selecting a random
map, a random race, at least one computer player, and the game starts,

•

Options – this section of the menu is used to configure sound (Audio) and picture
(Video) effects. Audio effects are shown in figure 4 – here you can set
the volume (Master volume), the volume of sounds and music while in the menu, and the volume
of unit sounds and background music played during the game itself.

Figure 4: Audio settings

The Video section (figure 5) provides settings, some of which take effect only after
restarting the application (marked *), while others require starting a new game to take effect
(marked **). Settings not marked with an asterisk take effect immediately. Available
settings are fullscreen and vertical synchronization (synchronizing
rendering with the monitor refresh rate). Texture filter represents the quality
of texture display when magnified and mipmap filter is the quality when minified,
•

Credits – provides information about the game's authors, the music and technologies used,

•

Quit – Quits the game; pressing the Escape key has the same effect.


Figure 5: Video settings

#### 2.3.2 Game screen

After the game starts, the user sees the playing field. The situation is shown in
figure 6. On the right side a menu is visible, which changes according to the selected unit
or according to the action being performed with a unit (shown later).
The menu can be hidden by clicking the small yellow square in the upper right corner of the screen.
In the lower right corner of the menu there is an icon representing the active segments, i.e. those
that are visible on the map. There are three segments in the game, and for units it can be defined in the configuration
files in which segment they may move, or for buildings, in
which segment they will stand. Switching between segments is done by clicking the
aforementioned icon in the lower right corner. The upper right corner is reserved for a miniature
map (radar map), which shows the current segment or segments and also the units
located in that segment. This minimap is a copy of the large game map. The gray frame
on the radar map marks the area currently visible on the large game map. In the middle part
of the menu, icons with actions that units can perform are visible. If no unit
is selected, these icons are unavailable (as is the case in figure 6). Before we
describe these actions, note the gray arrow-shaped mouse cursor in the lower part
of figure 6. The cursor shape changes during the game depending on the action being performed.


Figure 6: Playing field

Now to the unit actions themselves. Figure 7 shows this part of the menu once more. In
order, the icons represent the actions:
•

Stay – if the unit is moving or shooting, it stops
after clicking this icon,

•

Move – the unit moves to the place that
the user right-clicked after
selecting the unit,

•

Attack – the selected unit starts attacking another unit that was marked with the right
mouse button,

•

Mine – the selected unit goes to the resource the user right-clicked
and starts mining,

•

Repair – the selected unit goes to the building it is to repair and then starts
repairing it,

•

Build – the unit starts building the chosen building at the chosen location (if
possible).

Figure 7: Unit actions

The narrow bar at the bottom of the screen shows information about the materials that the player's
units have gathered (amount of gold, wood and coal), as well as the amount of food and energy.
The large game map shows the player's own as well as enemy units; the black area of the map
has not yet been explored and is covered by the fog of war, the so-called warfog.
After left-clicking a unit, the situation on the playing field and in the menu
changes (described by figure 7). The unit is selected, and above it (in the case of a building,
next to it) a colored bar appears indicating how much life the unit has left. This bar can
have three colors:
•

Green – indicates a life range of 100% - 70%,


•

Yellow – indicates a life range of 69% - 30%,

•

Red – less than 30%.

On the right side of the screen, in the middle of the menu, a picture of the selected unit is shown, with
its name and information about its current and maximum life. Further
detailed information is also displayed (figure 8):
•

Armor – shield strength and the ability to dodge,

•

Damage – minimum and maximum weapon
strength; after the '/' sign the radius of the destructive
area around the projectile impact is shown,

•

Range – minimum and maximum weapon range +
weapon hit accuracy,

•

View – the radius of the surroundings the unit
reveals around itself,

•

Max speeds – the maximum speed the unit can reach in the individual
segments,

•

Food/Energy – how much food or energy the unit takes from the given player's
stock when it is created,

•

Materials – symbols of materials the unit can mine; for resources it is the
material the resource provides,

•

Capacity – this information appears only for resources and indicates the current amount of the given
material in the resource.

Figure 8: Unit information

Not all units display all the information, e.g. units that are not
allowed to mine any material lack the Materials information, etc.
Below these descriptions in the menu are the allowed actions the unit can perform. All actions
have already been described (figure 8). In the darker part of the menu the unit defence types are visible
(figure 9), in order:
•

Stay – the unit remains motionless at its position
even when threatened by a foreign unit,

•

Guard – the unit starts an attack if an enemy unit is
within its range,

•

Offensive guard – like Guard; in addition, if the enemy unit moves away,
the defending unit starts pursuing it in order to continue the attack,

•

Agressive guard – similar to Offensive guard; in addition, the defending unit
runs after the enemy unit even if it cannot reach it with fire but can see it.

Figure 9: Defence menu

NOTE: If the user has selected several units at once, the menu
shows a picture of one unit type, the text "Multi selection", and as available are shown
only those actions that all units in the selection can perform.

#### 2.3.3 Actions with units

The following paragraph describes how actions can be performed with selected
units. During these actions the mouse cursor changes depending on whether the action is allowed or


forbidden. In the description we will often refer to the configuration files, which were described
in a separate part of the documentation. By actions we mean:
•

Walking – we select the unit with the left button and by right-clicking on the playing field
we set the destination. The unit immediately tries to reach the destination. If the destination
is not reachable, the unit tries to get at least as close to it as possible. Of course,
units can only move within the map; an attempt to move outside the map will be
rejected. It is also possible to select several units, which will form a formation,
and send them to some place on the map,

•

Mining – some units have the ability to mine a certain kind of material. This property
can be defined in the configuration file and during the game it is visible in the menu on the right
side, as described in the section about the menu. We select the unit and right-click
on a resource in which the unit can mine. The unit enters the resource and after
mining a certain amount of material it leaves the resource and heads to the nearest building
that accepts the material it mines. Again, if no such building exists, the unit
keeps the mined material, but remains standing next to the resource and reports the absence of an accepting
building. One unit can also mine several kinds of material,

•

Building construction – the ability to construct buildings is again defined for a unit
in the configuration
file. We click on the
unit,
then
in the menu on the right
side
we select
the action
"Build"
and in the lower part of the menu
we select
the type
of building
that
we want
to build.
Since constructing a
building requires
material, construction
reduces the
Figure 11: Construction start
reserves of the player who
will own the building. Information about how much and what material is needed
to construct a given type of building can be seen by the player by hovering the mouse over the picture
describing the building in the lower part of the menu while building (figure 10 describes
the start of construction - with the selected unit and information about the amount of required
material).
A lack of material
means that the building
will not be built. When
subsequently
moving
the cursor over the map
the cursor
icon
represents the building
being built.
The background
of the cursor changes to
green
and red
depending on whether
Figure 10: Building construction
at the place on the map


over which it is located, the building can be built or not (figure 11). Green
means a positive result. By right-clicking the map we mark the place
where the building will stand in the future. The building unit runs to this place
and starts constructing the building. Building is not a group action, which means that construction
can be started by only one unit, but others can come to help it by simply
selecting them and right-clicking on the building under construction. After completion the situation
is announced to the player. When building buildings that accept some mined material, this
ability of theirs becomes effective only after they are fully completed,
•

Building upgrades – very similar to construction; it is performed the same way up to determining the construction site
of the
building.
Upgrade buildings
are built on the buildings
they upgrade, not
on the map itself,
and when construction starts
they take over the life of
the unit
that
they upgrade (figure
12 describes a building
upgrade; in the menu the building
used for the upgrade
is framed with a yellow
frame).
Figure 12: Upgrade
"The price" of the unit being built, which the user can see in the menu when they briefly
rest the mouse on the picture of the unit being built, is only approximate and applies only if
the upgraded building has full life. If not, the price increases depending on the
current life of the upgraded unit,

•

Building repair – repair can be started by simply selecting a unit that
can repair the given type of building and then right-clicking on the
building. If the unit cannot repair the selected type of building, it understands this as
a request to come to the building. Similar to construction, repairing a building costs the player time
and material. The amount needed for repair depends on the damage of the unit and on its type,

•

Fighting – units capable of combat can start attacking their own as well as foreign units
by selecting them, choosing the fight action in the menu and then pointing with the right button
at the unit we want to hurt. If the target unit is out of range, the fighting unit tries to get to a distance from which it can hit it.
As with the other actions, the mouse cursor will be changed while selecting the target
unit. All properties regarding weapons, their spread or
effectiveness can again be defined in the configuration file,

•

Hiding units – the kind and number of units
that a given unit can hide is again
defined in the configuration file. A unit can
be nested into another unit by simply
selecting the unit and right-clicking
on the shelter unit (figure
13, the yellow arrow marks the target unit).
Depending on its size, the unit occupies a certain number of
places in the hiding unit. In connection
Figure 13: Hiding


with buildings, this function can be used to hide units from the enemy during
combat,
•

Unit production – it is no surprise that, like the previous
properties, this one
is also
defined
in the configuration
file. A unit
can thus in general
produce
other
units.
We select
the unit, then
in the menu we select
the action
"Build"
and the menu
offers us
which
units
the producing
unit can make.
Figure 14: Unit production
By clicking
on
the picture of the unit
we want to produce, these units will accumulate in the lower part of the menu, together
with a sequence number (figure 14). This part will also show information about
how far the production of the first unit in this queue has progressed (Progress). Each produced
unit again costs the player some material; it is taken from the player gradually and if
the material runs out, production of the unit is paused until there is
enough material to continue production. In this case a warning triangle
with the symbol of the missing material is displayed above the producing building.

#### 2.3.4 Meaning of keyboard shortcuts

##### 2.3.4.1 Keyboard shortcuts

NOTE: When experimenting with the keyboard shortcuts described below, you must have
the English keyboard layout set!
Some keys have been assigned a special meaning in Dark Oberon. Here is their
list:
•

F5 – shows the bottom segment,

•

F6 – shows the middle segment,

•

F7 – shows the top segment,

•

F8 – shows all segments,

•

F9 – changes the resolution to 640x480,

•

F10 – changes the resolution to 800x600,

•

F11 – changes the resolution to 1024x768,

•

Ctrl – the key helps with incremental selection of units. We select a group
of units, press Ctrl and can add further units to the already selected group,


•

Ctrl + number – combines the selected group of units into a group with the given number. This
number then appears on each of the units when selected (figure 15).
Group numbering can be redefined by clicking the unit again and pressing Ctrl
+ number. If a group of units with the given
number already exists, it will be dissolved and
a new group with the given number will be created
from the selected
units.
Between
groups selected
this way you can switch
by pressing the corresponding group number on
the keyboard.
This
way
of selecting
units can help with easier
manipulation of whole groups,

•

Alt + number – sets the group of units with the given
number as active and centers the playing field so that this group is in its middle,

•

Tab – hides and shows the panels. If the dark button in the upper left corner of the radar
was pressed, the radar remains always displayed,

•

U, u – allows the player to chat with other players in the lower part of the playing field.
The written message can be sent by pressing the Enter key. Messages can be written
even with the panels hidden.

•

Q – quit, ends the game,

•

G – low CPU mode, no rendering is performed, which results in low CPU usage,

•

H, h – stay (hold), if a unit is for example walking across the map, it can be stopped by selecting it and then
pressing this key. The remaining shortcuts are listed only for
information; their detailed description was given in section 3.2:

•

M, m – move, T, t – attack, I, i – mine, R, r – repair, B, b – build.

•

W, A, S, D – scroll the map up, left, down and right, the same as the arrow keys
(in the original game S and A were stay and attack).

Figure 15: Effect of the Ctrl+number shortcut

##### 2.3.4.2 Mouse movements

•

Movement – moving the mouse to the sides of the game screen scrolls the map,

•

Arrows or W, A, S, D – likewise scroll the map,

•

Alt + mouse movement – the mouse position stays the same, only the playing field underneath
it changes,

•

Middle mouse button – with the middle mouse button pressed, the map can be
grabbed and by subsequently moving the mouse the playing field is dragged in the direction of movement,

•

Mouse movement in the radar window – by pressing the left mouse button on the gray
rectangle in the radar window and then moving while keeping the
button pressed, you can move around the map in the game window,

•

Zoom – turning the mouse wheel changes the zoom level of the map view.
The same effect can be achieved with the +, - keys.

### 2.4 Configuration file config.cfg
All settings accessible from the main menu of the game are stored in a special configuration
file config.cfg. In addition, this file also contains settings that are not in the menu. If the file


does not exist, a new one with default values is created automatically when the game starts.
The default values are also used if some items are missing from the file.
A complete list of items with their types and allowed values follows. T means the type
of the item's parameter. Information about the format of configuration files can be found in the chapter
on configuration files.

#### 2.4.1 Application window settings

•

fullscreen – T: bool. Specifies whether the application should start in fullscreen mode
or in a window. true – fullscreen, false – window,

•

resolution – T: string. Resolution (dimensions) of the application window. Allowed values are:
“640x480”, “800x600”, “1024x768”, “1152x864”, “1280x1024”, “1600x1200”,

•

vert_sync – T: bool. Use of synchronizing rendering with the monitor refresh rate.
true – synchronization on, false – off.

#### 2.4.2 Video settings

•

texture filter – T: string. Quality of texture display when magnified. Allowed
values: “nearest”, “linear”,

•

mipmap_filter – T: string. Quality of texture display when minified. Allowed
values: “none”, “nearest”, “linear”,

•

warfog_color – T: 3 x byte. Warfog color by components (red, green, blue
component). Each component lies in the interval <0, 255>,

•

warfog_intensity – T: byte. Warfog intensity in the interval <0, 100>. 0 – fully
transparent warfog, 100 – fully opaque,

•

show_fps – T: bool. Specifies whether information about the number of
rendered frames per second should be displayed on screen,

•

show_disconnect_warning – T: bool. Specifies whether a warning dialog should be shown
after logging out from the server or ending the game,

•

max_frame_rate – T: int. Maximum allowed number of frames per second.
Integer value in the interval <0, 1000>,

•

map_move_speed – T: byte. Map scrolling speed. In the interval <1, 100>,

•

map_zoom_speed – T: byte. Map zoom in / zoom out speed.
In the interval <1, 100>.

#### 2.4.3 Audio settings

•

snd_master_volume – T: byte. Global volume setting in the interval <0, 100>,

•

snd_menu_music_volume – T: byte. Volume of music in the menu. Value
in the interval <0, 100>,

•

snd_menu_sound_volume – T: byte. Volume of menu sounds. Value in the
interval <0, 100>,


•

snd_game_music_volume – T: byte. Volume of music in the game. Value in the
interval <0, 100>,

•

snd_game_sound_volume – T: byte. Volume of sounds in the game. Value in the
interval <0, 100>,

•

snd_menu_music – T: bool. Specifies whether music should play in the menu,

•

snd_game_music – T: bool. Specifies whether music should play in the game,

•

snd_unit_speach – T: bool. Specifies whether unit voices should be enabled when they are
selected and during other actions.

#### 2.4.4 User settings

•

player_name – T: string. Player name,

•

address – T: string. Last used address for connecting to another player,

•

sensitivity – T: byte. Mouse sensitivity in the interval <0, 100>.

#### 2.4.5 •

Network settings
net_server_port – T: int. Server port in the interval <1024, 65535> used when
creating a new game.

### 2.5 Data files
All textures for the graphical interface of the menu and various panels in the game, as well as internal sounds
and music, are stored in data files located in the /dat directory. For these files
it holds that their internal structure is fixed and must not be changed. If the user
wants to change e.g. the menu background or individual buttons, they must follow exact rules.

#### 2.5.1 File fonts.dat

The file fonts.dat contains one texture group with one texture showing side by side
the individual letters of the alphabet and characters used when displaying text. It is not recommended to
change this texture in any way. Its form is too tightly bound to the application.

#### 2.5.2 File cursors.dat

The file cursors.dat contains textures for the mouse cursor. One texture group
containing 19 textures is stored here, in the following order:
1. default – basic cursor for the graphical interface,
2. select – cursor when it is over a unit,
3. select_plus – cursor when it is over a unit with the Ctrl key
pressed,
4. can_move – cursor for the walk action if allowed,
5. cant_move – cursor for the walk action if forbidden,
6. can_attack – cursor for the attack action if allowed,


7. cant_attack – cursor for the attack action if forbidden,
8. can_mine – cursor for the mine action if allowed,
9. cant_mine – cursor for the mine action if forbidden,
10. can_repair – cursor for the repair action if allowed,
11. cant_repair – cursor for the repair action if forbidden,
12. can_build – cursor for the build action if allowed,
13. cant_build – cursor for the build action if forbidden,
14. can_hide – cursor for the hide action if allowed,
15. cant_hide – cursor for the hide action if forbidden,
16. eject – cursor for the action of unloading hidden units,
17. circle_in – secondary texture showing arrows pointing inward from a circle.
It is used when the unit action is allowed and is related to a change of the unit's
position,
18. circle_out – secondary texture showing arrows pointing out of a circle. It is used
when unloading hidden units,
19. cross – secondary texture showing a prohibition cross for the case of a forbidden action.

#### 2.5.3 File gui.dat

Textures and sounds of the graphical interface are located in the file gui.dat. There are six texture groups:
1. panels – contains the menu background (bg_menu), the background for the main game panel (bg_panel)
and the texture for the Credits dialog (bg_credits),
2. action_buttons – button textures for the individual actions. Their meaning is obvious
from the identifier. In order they are: bt_stay, bt_move, bt_attack, bt_mine, bt_repair,
bt_build. They are followed by textures for these buttons in the pressed state,
3. menu_buttons – here are the textures for the buttons of the individual menus and dialogs:
bt_play, bt_quic_play, bt_options, bt_credits, bt_quit, bt_resume, bt_create,
bt_connect, bt_disconnect, bt_back, bt_video, bt_audio, bt_ok, bt_yes, bt_no,
bt_cancel,
4. segment_buttons – buttons for displaying the various segments: bt_segment_0,
bt_segment_1, bt_segment_2, bt_segment_all,
5. labels – captions used in dialogs: lbl_display, lbl_resolution, lbl_texture_filter,
lbl_mipmap_filter, lbl_master, lbl_menu, lbl_game, lbl_map, lbl_info, lbl_name,
lbl_ip, lbl_players,
6. guard_buttons – buttons for changing the guard type: bt_ignore, bt_guard, bt_offensive,
bt_aggressive.
The sounds are the following:
1. menu_music – music for the menu,
2. button_click – sound for clicking a menu button,
3. game_music – music for the game,


4. component_click – sound for clicking an active menu element such as a check
box, etc.

### 2.6 Glossary of terms used
•

race – kind of the player's units (for example humans, giants, aliens...). Each player on the
map must have a unique race,

•

scheme – the environment in which the game takes place (for example the moon). The scheme
defines all the environment elements from which a map will be composed (one lunar
crater),

•

mapel – the smallest addressable unit of the map. The term was created in a similar
way to the word "pixel" = picture element, so "mapel" = map element.
Many unit properties in the configuration files are given in mapels,

•

fragment – a scheme element from which the map surface is formed. Fragments are
mapped onto the whole surface of one map layer and determine the height and difficulty of each
mapel,

•

segment – one layer of the map. The map has 3 layers: "underground", "surface" and "air".
Units can move in all three layers. Each of the layers has its own
textures and its own fragments,

•

warfog – an area of the map that the player has discovered at some point, but which none of their units
can currently see,

•

texture – a two-dimensional image used by the graphics card,

•

hyperplayer – a fictitious player who "owns" the scheme units, buildings and above all
resources. Its units and buildings have no function (other than a "decorative" one). From its
resources, however, the other players mine the materials needed for development,

•

leader (network) – the computer that creates the game. The other players connect to it. The leader
owns all computer players – including the hyperplayer,

•

follower (network) – a computer that connects to an already created game,

•

PathFinder – a function used to find a path in the three-dimensional map,

•

leader (path finding) – the unit that was chosen as the "leader" of a group of units
during group walking. For optimization and "keeping the formation of the group of units",
the path in the map is searched only once – exactly for the leader – the other units just
take it over,

•

event (message queue) – a message type used by the message queue. An event changes the state of the unit
to which it is addressed,

•

request (message queue) – a message type used by the message queue. A request is a request
to perform some action and does not change the state of the unit to which it was addressed.


## 3 Creating a game
Martin Košalko


### 3.1 Configuration files
The Dark Oberon project is not a single specific strategy game with firmly defined
units, environment and maps. In its essence, the project is a computing engine
(template) for various instances of strategy games of the Warcraft II (C&C) type, which can be
defined precisely by means of configuration files. To successfully create a specific game, it is
necessary to define the unit types together with their properties (the race), the environment in which
these types will move (the scheme) and finally to compose a specific map from the environment
elements. The meaning and mutual relations of the individual file types are best seen in an example:
•
•
•

Scheme – lunar landscape
Races – astronauts, aliens
Map – one specific lunar crater

A more precise description follows:
• Scheme - At first glance it is clear that the basis of every game is the scheme, i.e.
the environment in which the game will take place. The scheme does not depend on other
files, but a so-called scheme race must exist for it. It is a race specific to
the given environment – in our example these could be the lunar "natives" together with their
natural resources. For the game itself, the units and buildings of the scheme
race are not important; they serve a more or less decorative function. What matters are the natural resources from
which the other races will mine the materials needed for their development.
•

Races - Races depend on the scheme. The header of a race definition must list
the schemes (at least one valid) in which this race can be used. It is hard to
imagine, for example, divers fighting on the lunar surface. The dependence of a race
on a scheme consists for example in the definition of surface types where a unit can
move, or in the definition of material types a unit can mine.

•

Maps - A map is created using elements defined in the scheme (and is therefore
dependent on the scheme). Similarly to races, the map header states the scheme from which
the building elements are to be taken. Unlike races, however, exactly
one scheme must be given. The map forms the game board in which the players (whether computer or
real) appear. Since the program cannot in any way graphically distinguish the units of two different
players with the same race, each player on the map must have a different race. Therefore
the map defines, besides the maximum number of players, also the races from which one can choose.
If fewer races than the maximum number of players are listed in the map, it will not be possible
to select the maximum number of players for the game, but only a number equal to the number of listed races.
Each race listed in the map must of course be usable in the map's scheme.

#### 3.1.1 Common features of loaded items and abbreviations used

•

textures – If a value (a textual identifier of a texture group) is given which
does not exist in the data file of the corresponding race, the program reports an error. Exactly
one texture group identifier is given.

•

sounds - The value “none” can be given, which means that no sound will be used.
However, if a value is given which does not exist in the data file of the corresponding race,
the program reports an error. Several identifiers can be given; in the game one of them
is always chosen at random.


In the description of the individual items of the configuration files, abbreviations
with the following meaning are used in parentheses:
• T (type) - expected variable type (string, integer, byte, bool, float),
• R (range) – expected range of values,
• D (default value) – if a wrong type, range, or invalid value is given,
the predefined value will be used,
• N (necessary item) – specifies whether the item is mandatory.

#### 3.1.2 Description of scheme files

Scheme files have the extension ".sch" and their unique identifier, used as a reference
in the other configuration files, is the file name without the extension. As already mentioned,
a so-called scheme race must exist for each scheme. All available scheme files (together
with the scheme race files) are located in the "schemes" subdirectory of the game's root
directory. The scheme defines the environment in which the game will take place. The game environment can
be composed step by step from several types of elements. The smallest terrain element (component
of the environment) is the terrain type - "Terrain type". Terrain types are mapped, by means of square groupings
of terrain types – "Fragments", onto the smallest addressable units of the map –
"Mapels". Three layers of the environment – "Segments" – are composed of mapels. There are always 3 segments
and they represent the subsurface layer (underground), the surface layer (ground) and the above-surface
layer (air).
Of course, mapels are not defined in the scheme,
but they are mentioned here
for a better
understanding
of the relations
Segment 2
between
the elements
Segment 1
of the environment. Similarly,
the scheme does not define
Segment 0
the final
form
of a segment either (that is defined,
of course,
in the configuration file
Fragment
of the map), but in the scheme
for each segment
Mapel
the fragment types
and terrain types
are defined.
Besides the environment elements, it contains the definition of material types and the scheme header with
basic information about the scheme.

##### 3.1.2.1 Header

The header contains basic information about the scheme.
•

name – name of the scheme. The program itself does not use it in any way; it is only information.
(T: string, R: 1024 characters, D: „“, N: no),

•

author – name of the file's author. It is not used in the program; it is just extra information.
(T: string, R: 1024 characters, D: „“, N: no).


name "Plastic World"
author "PP team"

##### 3.1.2.2 Section <Materials>

The section contains definitions of material types. At most 4 material types can be
defined, but at least one must be loaded successfully. The section contains the following items
and subsections:
•

count – number of material types. The program tries to read the given number of
<Material> sections. If it fails, loading of the scheme ends with an error. (T: integer,
R: maximum number of material types = 4 >= r >= 1, D: 1, N: yes),

•

<Material> - subsection containing the definition of one material type.
o id – unique textual identifier of the material type. (T: string, R: 1024
characters, D: „Material X“ – section name, N: no),
o name – name of the material displayed in the game as a description. (T: string, R: 1024
characters, D: id – material identifier, N: no),
o tg_id – for the material, the item specifies the textual identifier of the texture group
(defined in the data file of the corresponding scheme) of the material picture. It is used
as an icon when displaying the player's current material amount and as an identifier
of a material shortage. (T: string, R: 1024 characters, D: -, N: yes).

<Materials>
count 1
<Material 0>
id "gold"
name "Gold"
</Material 0>
</Materials>

##### 3.1.2.3 Sections <Segments> and <Segment>

The only task of the <Segments> section is to wrap 3 subsections <Segment 0>, <Segment 1> and
<Segment 2>. All important information is in these subsections, where the
definitions of the environment can be found.
•

name – name of the segment. (T: string, R: 1024 characters, D: „Segment X“ – section
name, N: no),

•

count – number of terrain types of the given segment. The program tries to load the given
number of <Terrain type> sections. If it fails, loading of the scheme ends with an error.
(T: integer, R: >= 0, D: 0, N: yes),

•

<Terrain type> - subsection containing detailed information about a terrain type in the given
segment,

•

<Fragments> - subsection containing the fragment definitions of the given segment,

•

<Layers> - subsection defining layers – special terrain elements that can be
placed anywhere in the map and locally change the properties of the surface on which they are
placed. Layers may also overlap each other and are not always drawn under
the units in the map. A good example is a swamp, which is always under the units and changes
the terrain difficulty,


•


<Objects> - subsection defining terrain elements similar to layers, but objects
cannot overlap each other and are included in the texture sorting algorithms. An
example is a tree.

<Segment 1>
count 9
name "Earth"
<Terrain type 0>
…
</Terrain type 0>
<Fragments>
…
</Fragments>
<Layerss>
…
</Layers>
<Objects>
…
</Objects>
</Segment 1>

##### 3.1.2.4 Section <Terrain type>

The section contains the exact definition of a terrain type.
•

name – name of the terrain type. (T: string, R: 1024 characters, D: „Terrain type X“ –
section name, N: no),

•

layer – layer of the terrain type. The layer can be understood as the terrain height. It is precisely
by means of an interval of two layers that it is defined where a unit can walk - it is thus
possible to define that a unit can move across all layers (for a better
idea, the word heights fits well) from the given interval. (T: integer, R: >= 0, D: 0,
N: no),

•

dificulty – the item defines how difficult it is to move across the given surface. Expected
is a number from the interval <0, 1>, where 0 means that the unit's movement speed equals
the unit's maximum speed in the given segment, and with an increasing value the
unit's speed decreases in direct proportion, up to 1, which means that the unit cannot move
on the given surface at all. (T: float, R: 1>= r >= 0, D: 0, N: no).

<Terrain type 6>
name "Rocks"
layer 30
difficulty 0
</Terrain type 6>

##### 3.1.2.5 Section <Fragments>

The section contains definitions of named square groupings of terrain types.
•

tg_default_id - textual identifier of the texture group (defined in the data file of
the corresponding scheme) of the picture of the default terrain type with a size of 1x1 mapel.
It is used if the whole map is not covered by fragments – the empty places are filled
with it. (T: string, R: 1024 characters, D: -, N: no),


•

default_terrain_id – layer of the default terrain type with a size of 1x1 mapel,
which fills the places not covered by fragments. The value „none“ can be given, which
means that the terrain type will be transparent. (T: integer, R: >= 0, D: 0, N: yes),

•

count – number of fragment types of the given segment. The program tries to load the given
number of <> sections. If it fails, loading of the scheme ends with an error. (T: integer,
R: >= 0, D: 0, N: yes),

•

<Fragment> - subsection defining the properties of one fragment.

<Fragments>
tg_default_id "default"
default_terrain_id 10
count 80
<Fragment 4>
…
</Fragment 4>
</Fragments>

##### 3.1.2.6 Section <Fragment>

The section contains the definition of one specific fragment.
•

size – size of the fragment in mapels. A fragment is always square. (T: byte, R: >= 1,
D: 1, N: no),

•

tg_id – textual identifier of the texture group (defined in the data file of
the corresponding scheme) of the fragment surface. The value „none“ can be given, which means
that no texture will be used and the fragment will be fully transparent. (T: string, R: 1024
characters, D: -, N: yes),

•

terrain_id – size*size heights (layers) of terrain types are expected, which determine
the height of the given part of the fragment. If too few values are given, loading of the scheme ends
with an error. If a value of a non-existent layer is given, the nearest
higher existing layer is taken into account. (T: integers, R: >= 0, D: 0, N: yes).

<Fragment 4>
size 3
tg_id "rocks_se"
terrain_id 30 30 30 30 30 10 30 30 30
</Fragment 4>

##### 3.1.2.7 Sections <Layers> and <Objects>

•

count – number of objects (layers) to be loaded. If there are not enough of them,
loading of the scheme ends with an error. (T: integer, R: >= 0, D: 0, N: yes),

•

<Object>, <Layer> - definitions of the individual objects and layers.

<Layers>
count 1
<Layer 1>
…
</Layer 1>
</Layers>


##### 3.1.2.8 Sections <Layer> and <Object>

The environment elements <Layer> and <Object> have completely identical properties, but are understood slightly
differently. While a layer is understood as "two-dimensional", an object is "three-dimensional". Two layers
placed on the map may overlap (the terrain is modified by the one added last)
and they are always drawn under the units of the same segment. Two objects must not overlap
and their textures are sorted like all other units. Both objects and layers serve for local
changes of the terrain.
•

id – textual identifier of the object (layer), which is used as a reference to the type
in the other configuration files.. (T: string, R: 1024 characters, D: „Object X“,
N: no),

•

name – name of the object. (T: string, R: 1024 characters, D: id – object identifier,
N: no),

•

width – width of the object's (layer's) base in mapels. (T: byte, R: >= 1, D: 1, N: no),

•

height – length of the object's (layer's) base in mapels. (T: byte, R: >= 1, D: 1, N: no),

•

tg_id – textual identifier of the texture group (defined in the data file of the corresponding
scheme) of the given object (layer). (T: string, R: 1024 characters, D: -, N: yes),

•

terrain_id – the item has the same meaning as the item of the same name in the section
<Fragment>.

<Object 0>
id "tree"
name "Tree"
width 2
height 2
tg_id "o_tree"
terrain_id 10 10 10 10
</Object 0>

#### 3.1.3 Description of race files

Race files have the extension ".rac" and their unique identifier used as a reference
in the other configuration files is the file name without the extension. The content of the file and its
location in the directory structure depend on whether the file defines a scheme race or a player
race. A scheme race is located in the "schemes" subdirectory of the game's root directory and the file
name (except for the extension) must be identical to the name of the scheme file. The other (player) race files
are located in the "races" subdirectory of the game's root directory, where for each race
a further subdirectory with the same name as the race is expected. The main reason for the existence of
race configuration files is the possibility to define kinds of units, buildings and resources and their
detailed properties. Besides that, the file contains a header with information about the race as such.
The difference in content between player races and the scheme race lies in the definition of resources in the scheme race.

##### 3.1.3.1 Header

The section contains data valid for the whole race and formal information about the file.
•

name – name of the race. The name given in this item is displayed in the menu when choosing a race.
(T: string, R: 1024 characters, D: race file name, N: no),


•

author – name of the file's author. It is not used in the program; it is just extra information.
(T: string, R: 1024 characters, D: „“, N: no),

•

schemes – the item expects textual identifiers of the schemes under which the
race can be used. If no scheme is given, the race cannot be selected
in the menu. (T: string, R: 1024 characters, D: „“, N: no),

•

tg_food_id, tg_energy_id – textual identifier of the texture group (defined
in the data file of the corresponding race) of the food (energy) picture – it is used as an icon when
displaying the player's current food (energy) amount and as an identifier of a food
(energy) shortage. (T: string, R: 1024 characters, D: -, N: yes),

•

tg_burning_id – textual identifier of the flame texture group defined in the data
file of the corresponding race. The texture is used for buildings and resources that do not have
their own tg_burning_id item defined. (T: string, R: 1024 characters, D: -,
N: yes),

•

snd_burning – textual identifier of the sound of a burning unit (defined in the data
file of the corresponding race). The sound is used for buildings and resources that do not have
their own sound defined. It is played at the moment a burning unit is selected.
(T: string, R: 1024 characters, D: -, N: yes),

•

snd_dead – textual identifier of the sound of a dying movable unit. The sound is
used if the unit does not have the snd_dead item defined. (T: strings,
R: 1024 characters, D: -, N: yes),

•

snd_explosion – textual identifier of the sound of an exploding (collapsing) building
or resource. The sound is used only if the building (resource) does not have
the snd_explosion item defined. (T: strings, R: 1024 characters, D: -, N: yes),

•

snd_error – textual identifier of the sound (defined in the data file) that is
used on an unsuccessful attempt to perform some action with the cursor. (T: string,
R: 1024 characters, D: -, N: yes),

•

snd_placement – textual identifier of the sound used when placing a building
into the map with the mouse cursor. (T: string, R: 1024 characters, D: -, N: yes),

•

snd_construction – textual identifier of the sound used when a building under construction
changes texture, or when a building under construction is selected. (T: string, R: 1024 characters,
D: -, N: yes),

•

snd_building_selected – textual identifier of the sound used when a building
or resource is selected, and only if the building (resource) does not have its own item
snd_selected defined. (T: string, R: 1024 characters, D: -, N: yes),

name "Plastic Humans"
author "[:-)] PP team"
schemes "plastic"
tg_food_id "food"
tg_energy_id "energy"
tg_burning_id "building_burning"
snd_error error
snd_placement placement
snd_construction construction
snd_burning burning


snd_dead dead
snd_explosion explosion1 explosion2 explosion3
snd_building_selected building_selected

##### 3.1.3.2 Section <Units>

The section contains definitions of the types and properties of movable units. According to the actions that
movable units can perform, we divide them into two groups: fighters (FORCE UNITS)
and workers (WORKER UNITS), who can additionally mine materials from resources, construct buildings
and repair them. The section contains the following items and sections:
•

count - number of movable unit types of the race. The program tries to read the given
number of <Unit> sections. If it fails, loading ends with an error. (T: integer,
R: >=0, D: 0, N: yes),

•

<Unit X> - definition of one specific movable unit type.

<Units>
count 10
<Unit 0>
…
</Unit 0>
</Units>

##### 3.1.3.3 Section <Unit>

The section contains the definition of a unit type.
Common properties of fighters (FORCE UNIT) and workers (WORKER UNIT):
•

id – textual identifier of the unit type used for its unique identification.
(T: string, R: 1024 characters, D: „Unit X“ = name of the current section, N: no),

•

name – name of the unit kind used as a description in the game. (T: string, R: 1024
characters, D: id = unit type identifier, N: no),

•

size – width and height of the unit kind in mapels, i.e. the smallest addressable
units of the area (T: byte, R: 15 >= r > 0, D: 1, N: no),

•

materials – amount of materials needed to produce (train) the unit
type. At least as many numbers must be given as materials were defined
in the corresponding scheme. (T: floats, R: >= 0, D: 0, N: no),

•

max_life – maximum life value of the unit type (T: integer, R: >= 1, D: 1,
N: no),

•

max_speed – maximum speed value of the unit type for each segment in
mapels per second. Three numbers are expected (number of segments) (T: floats, R: >=
0.01, D: 0.01, N: no),

•

max_rotation_speed – maximum rotation speed value of the unit type for
each segment in degrees per second. Three numbers are expected (number of segments)
(T: integers, R: >= 1, D: 1, N: no),

•

selection_height – height of the frame marking the unit in pixels (T: byte, R: >=
0, D: 40, N: no),


•

burning_position – X and Y relative coordinate of the flame with respect to the unit in
pixels. 2 values are expected. (T: float,float, R: -, D: 0,20, N: no),

•

item_type – the item denotes the unit type. One of the values is expected: “f”- fighter
(FORCE UNIT), „w“- worker (WORKER UNIT). If the value is „w“,
the items specialized for workers will additionally be loaded. (T: string, R: 1 character,
D: „f“, N: yes),

•

view – sight range of the unit from its edge in mapels. (T: byte, R: MAX_MAP_SIZE
= 240 >= r >= 1, D: 1, N: no),

•

energy – value stating how much energy each unit of the given type adds (positive value) or
needs (negative value). (T: integer, R: -, D: 0,
N: no),

•

food – value stating how much food each unit of the given type adds (positive value) or needs
(negative value). (T: integer, R: >= 0, D: 0, N: yes),

•

move_terrain_id - minimum and maximum terrain height where the unit can
move in the given segment. Three (number of segments) pairs of numbers are expected.
The program checks whether the loaded minimum is smaller than the maximum (if not,
the minimum is taken as the maximum). (T: integers, R: >= 0, D: 0, N: no),

•

land_terrain_id - minimum and maximum terrain height where the unit can land
in the given segment (but not move). Three (number of segments)
pairs of numbers are expected. The program checks whether the loaded minimum is smaller than the maximum
(if not, the minimum is taken as the maximum). (T: integers, R: >=0, D: 0, N: no),

•

min_exists_segment – index of the lowest segment where the unit can exist.
(T: byte, R: number of segments = 3 > r >= 0, D: 0, N: no),

•

max_exists_segment - index of the highest segment where the unit can exist.
(T: byte, R: number of segments > r >= min_exists_segment, D: 0, N: no),

•

min_max_visible_segment_id – for each segment a defined pair of the minimum
and maximum segment index where the unit can see. Three (number of
segments) pairs of indices are expected, and it is checked that the loaded minimum is
smaller than the maximum. (T: bytes, R: number of segments = 3 > r >= 0, D: 0, N: no),

•

land_segment_id – index of the segment the unit gets to when it stops. The unit is
primarily added to this segment after being produced. (T: byte, R: number of segments = 3 > r >=
0, D: 0, N: no),

•

max_hided_units – maximum number of space units that the unit is able to carry
inside itself. One space unit corresponds to a combat unit with a size of 1
square mapel. (T: byte, R: >=0, D: 0, N: no),

•

can_hide – list of textual identifiers of unit types that the given type can carry
inside itself. (T: strings, R: 1024 characters, D: „“, N: no),

•

tg_picture_id – textual identifier of the texture group of the unit's picture in the menu,
defined in the data file of the corresponding race. (T: string, R: 1024 characters, D: -,
N: yes),

•

tg_stay_id – textual identifier of the texture group of a standing unit, defined in
the data file of the corresponding race. (T: string, R: 1024 characters, D: -, N: yes),


•

tg_anchor_id – textual identifier of the texture group of an anchored (landed) unit
defined in the data file of the corresponding race. The value “none” can be given, which
means that the tg_stay_id texture will be used. (T: string, R: 1024 characters, D: -, N: yes),

•

tg_move_id – textual identifier of the texture group of a moving unit
defined in the data file of the corresponding race. The value “none” can be given, which
means that the tg_stay_id texture will be used. (T: string, R: 1024 characters, D: -, N: yes),

•

tg_land_id – textual identifier of the texture group of a landing and taking-off
unit defined in the data file of the corresponding race. The value
“none” can be given, which means that the tg_stay_id texture will be used. (T: string, R: 1024 characters,
D: -, N: yes),

•

tg_rotate_id – textual identifier of the texture group of a rotating unit defined in
the data file of the corresponding race. The value “none” can be given, which means that
the tg_stay_id texture will be used. (T: string, R: 1024 characters, D: -, N: yes),

•

tg_attack_id – textual identifier of the texture group of an attacking unit defined in
the data file of the corresponding race. The value “none” can be given, which means that
the tg_stay_id texture will be used. (T: string, R: 1024 characters, D: -, N: yes),

•

tq_dying_id – textual identifier of the texture group of a dying unit defined in
the data file of the corresponding race. The value “none” can be given, which means that
this unit state is skipped. (T: string, R: 1024 characters, D: -, N: yes),

•

tg_zombie_id – textual identifier of the texture group of a dead (decomposing)
unit defined in the data file of the corresponding race. The value
“none” can be given, which means that this unit state is skipped. (T: string, R: 1024 characters, D: -,
N: yes),

•

tg_projectile_id – textual identifier of the projectile texture group defined in the data
file of the corresponding race. The value “none” can be given, which means that no
texture is used and the projectile will be invisible. (T: string, R: 1024 characters, D: -, N: yes),

•

tg_burning_id – textual identifier of the flame texture group defined in the data
file of the corresponding race. The value “none” can be given, which means that no
texture is used. (T: string, R: 1024 characters, D: -, N: yes),

•

snd_ready – textual identifier of the sound (from the data file) of a unit that has finished
an action and is ready for the next one. (T: strings, R: 1024 characters, D: -, N: yes),

•

snd_selected - textual identifier of the sound (from the data file) of a unit that has
just been selected. (T: strings, R: 1024 characters, D: -, N: yes),

•

snd_command - textual identifier of the sound (from the data file) of a unit that has
just been sent to some action. (T: strings, R: 1024 characters, D: -, N: yes),

•

snd_fireon - textual identifier of the sound (from the data file) of a unit that is loading.
(T: strings, R: 1024 characters, D: -, N: yes),

•

snd_fireoff - textual identifier of the sound (from the data file) of a unit that has just
fired. (T: strings, R: 1024 characters, D: -, N: yes),

•

snd_hit - textual identifier of the sound (from the data file) of a projectile impact.
(T: strings, R: 1024 characters, D: -, N: yes),


•

snd_burning - textual identifier of the sound (from the data file) of a burning unit.
(T: strings, R: 1024 characters, D: -, N: yes),

•

snd_dead - textual identifier of the sound (from the data file) of a dying unit. The
value „none“ can be given, which causes the sound defined by the item
snd_dead in the race header to be used. (T: strings, R: 1024 characters, D: -, N: yes),

•

is_offensive – if this item has the value true, the offensive properties of the
unit will also be loaded, otherwise not. (T: bool, R: -, D: false, N: no),

•

offensive_aggressivity – the item determines the default degree of aggressiveness of units
of the given type. The item can take the following values: “aggressive”- the unit
attacks an enemy unit at the moment it has it within sight and will
pursue it, “offensive”- the unit attacks an enemy unit at the moment it
can reach it with fire and will pursue it, “guarded”- the unit attacks an enemy
unit at the moment it can reach it with fire, but will not pursue it (it stays standing in
place), “ignore”- the unit completely ignores foreign units. (T: string,
R: „aggresive“, „offensive“, „guarded“, „ignore“, D: „ignore“, N: no),

•

offensive_accuracy – weapon accuracy. Given as a number from the interval <0,1>, where
0 means inaccurate (at a distance of 10 mapels the maximum deviation is 3 mapels), 1
means accurate (in the absolute sense). (T: float, R: 1>= r >=0, D: 0, N: no),

•

offensive_range – minimum and maximum distance in mapels, counted from the
edge of the unit, at which the unit is able to shoot. The program checks whether
the loaded minimum is smaller than the maximum (if not, the maximum is modified).
Two numbers are expected. (T: bytes, R: >=1, D: 0, N: no),

•

offensive_scope – radius of the projectile explosion on impact in mapels. (0 means 1x1
mapel, 1 means 3x3 mapels ...) (T: byte, R: >= 0, D: 0, N: no),

•

offensive_shotable_seg_min_max – index of the minimum and maximum segment
the unit can reach with fire. It is checked whether the index of the minimum segment is smaller than
the index of the maximum segment (if not, the index of the maximum one is modified).
A pair of numbers is expected. (T: bytes, R: number of segments = 3 > r >= 0, D: 0, N: no),

•

gun_power_min_max – minimum and maximum attack strength. The minimum attack strength
is always guaranteed, and a random strength is added to it so that the
maximum attack strength is not exceeded. A pair of numbers is expected, and it is checked that
the loaded minimum number is smaller than the maximum. (T: integers, R: >= 0, D: 0,
N: no),

•

gun_shot_speed – projectile speed in mapels per second. (T: float, R: >= 0.01,
D: 0.01, N: no),

•

offensive_shot_time – time the unit needs for one shot (from pulling
the trigger to the moment the projectile leaves the barrel). (T: float, R: >= 0, D: 0, N: no),

•

offensive_wait_time – time for which the unit is "unusable" after firing (time
needed for the barrel to cool down after a shot before it can be loaded again).
(T: float, R: >= 0, D: 0, N: no),

•

offensive_feed_time – time needed to load the weapon. (T: float, R: >= 0, D: 0,
N: no),


•

offensive_flags – the item determines special weapon properties. Several
flags are expected, or the flag “none”, which says that the weapon has no
special properties. Expected values: “gun_same_segment”- the unit can
attack only units in the segment where it is currently located, “gun_damage_buildings”- the unit's attack also threatens buildings, “gun_damage_sources”- the unit's attack also threatens
resources, “gun_notlanding”- the unit cannot shoot while anchored (after landing).
(T: strings, R: 1024 characters, D: „none“, N: no),

•

defence_armour – the item expresses defence strength and is the counterpart of attack strength (T: integer, R: >= 1, D: 1, N: no),

•

defence_protection – the item expresses the unit's ability to dodge an attack.
A number between 0 (the unit does not dodge) and 1 (the unit dodges) is expected. If the unit has
the ability to dodge, the attack on it is reduced by a small random number. (T: float, R:1>=
r >= 0, D: 0, N: no),

•

features – the item determines special properties of the unit type. Several
flags are expected, or the flag “none”, which says that the unit has no
special properties. Expected values: “have_to_land”- the unit cannot remain standing,
but must land (if it cannot, its life is reduced at regular intervals),
“heal_when_stay”- the unit can heal itself when standing (the healing time
is set by the item heal_time), “heal_when_anchor”- the unit heals itself when
anchored (as in the previous case, the recovery time is set by the item
heal_time). (T: strings, R: 1024 characters, D: „none“, N: no),

•

heal_time – time needed to heal by one unit of life, in seconds. It is taken
into account only if the unit has one of the following special
properties: “heal_when_stay”, “heal_when_anchor”. (T: float, R: >= 0, D: 0, N: no),

Special properties of a worker (WORKER UNIT): they are loaded only if the item
item_type has the value „w“.
•

max_amount – maximum amount of materials the worker is able to
carry. As many numbers are expected as materials were defined
in the corresponding scheme. (T: integers, R: >= 0, D: 0, N: no),

•

allowed_materials – a list of textual identifiers is expected, which
determines which materials the worker can mine. In case of a wrong textual
identifier, the program reports a warning, but this does not interrupt loading. (T: strings,
R: 1024 characters, D: „“, N: no),

•

mining_time – time needed to mine one unit of material from a resource, for
each material. As many numbers are expected as materials were
defined in the corresponding scheme. Unloading the given material is always 10 times
slower. (T: floats, R: >= 0, D: 0, N: no),

•

mining_sound_shift – for each material type the item defines the time offset
of playing the first sound from the start of mining, in seconds. As many
numbers are expected as materials were defined in the corresponding scheme. (T: floats, R: >=
0, D: 0, N: no),

•

mining_sound_time – for each material type the item defines the time between two
sounds of the same worker in seconds. As many numbers are expected as materials were
defined in the corresponding scheme. (T: floats, R: >= 0, D: 1, N: no),


•

snd_mine_materialX – textual identifier of the sound (from the data file) of a mining
unit for material X. (T: strings, R: 1024 characters, D: -, N: yes),

•

snd_workcomplete – textual identifier of the sound (from the data file) of a worker
that has finished an action. (T: strings, R: 1024 characters, D: -, N: yes),

•

tg_mine_id – textual identifier of the texture group of a mining unit defined in
the data file of the corresponding race. The value “none” can be given, which means that
the texture of a standing unit is used. (T: string, R: 1024 characters, D: -, N: yes),

•

tg_repair_id – textual identifier of the texture group of a repairing and constructing
unit defined in the data file of the corresponding race. The value
“none” can be given, which means that the texture of a standing unit is used. (T: string, R: 1024
characters, D: -, N: yes),

•

repairing_time – the item defines the time needed to repair (construct) one
unit of life of the building the worker is repairing (constructing), in seconds. (T: float,
R: >= 0, D: 0, N: no),

•

can_build – a list of textual identifiers of buildings the worker can
build is expected. It can also automatically repair all the listed buildings. If
an incorrect building identifier is given, the program displays a warning.
(T: strings, R: 1024 characters, D: „“, N: no),

•

can_repair - a list of textual identifiers of buildings and units
the worker can repair is expected. It is not necessary to list buildings the
worker can build, because it can automatically repair those as well. If
an incorrect building (unit) identifier is given, the program displays a warning.
(T: strings, R: 1024 characters, D: „“, N: no).

<Unit 1>
id "peasant"
name "Peasant"
size 1
view 6
energy 0
food –10
materials 400 5 0
max_life 70
max_speed 10 10 10
max_rotation_speed 1440 1440 1440
selection_height 35
burning_position 0 0
item_type w
max_amount 100 20 0
allowed_materials "gold" "wood"
can_build "farm" "tower" "cannontower" "barracks" "shed"
can_repair "catapult" "airship"
mining_time 0.1 0.33 0
repairing_time 0.2
mining_sound_shift 0 0 0
mining_sound_time 0 1.0 0
move_terrain_id 0 0 8 25 0 0
land_terrain_id 0 0 0 0 0 0


min_exist_segment_id 1
max_exist_segment_id 1
min_max_visible_segment_id 1 2 1 2 1 2
land_segment_id 1
max_hided_units 0
can_hide
tg_picture_id peasant_picture
tg_stay_id peasant_stay
tg_anchor_id none
tg_move_id peasant_stay
tg_land_id none
tg_rotate_id none
tg_attack_id peasant_stay
tg_mine_id peasant_stay
tg_repair_id peasant_stay
tg_dying_id none
tg_zombie_id none
tg_projectile_id none
tg_burning_id none
snd_ready peasant_ready
snd_selected peasant_selected1 peasant_selected2
snd_command peasant_command1 peasant_command2
snd_workcomplete peasant_workcomplete
snd_mine_material0 none
snd_mine_material1 peasant_chop1 peasant_chop2 peasant_chop3
snd_mine_material2 peasant_chop1 peasant_chop2
snd_mine_material3 none
snd_fireon none
snd_fireoff peasant_hit
snd_hit none
snd_burning none
snd_dead none
is_offensive true
offensive_aggressivity guarded
offensive_accuracy 0.8
offensive_flags none
offensive_range 1 1
offensive_shot_time 0
offensive_wait_time 0
offensive_feed_time 0.8
offensive_scope 0
offensive_shotable_seg_min_max 1 1
gun_power_min_max 5 20
gun_shot_speed 100
defence_armour 2
defence_protection 0.3
features heal_when_stay
heal_time 5.0
</Unit 1>

##### 3.1.3.4 Section <Buildings>

The section contains definitions of the properties of the race's static units – buildings (BUILDING UNITS)
and factories (FACTORY UNITS). Static units are divided into the two mentioned types according to


the activities they can perform (factories can additionally produce (train) the race's movable
units). It contains the following items and subsections:
•

count - number of static unit types of the race. The program tries to read the given
number of <Building> sections. If it fails, loading ends with an error. (T: integer,
R: >=0, D: 0, N: yes),

•

<Building X> - definition of one specific static unit type.

<Buildings>
count 5
<Building 0>
…
</Building 0>
</Buildings>

##### 3.1.3.5 Section <Building>

In this section, detailed definitions of building properties can be found. Many items are identical
to the items of the <Unit> section (both in text and in meaning) and will therefore only be listed.
The other items are described in detail in the following list:
•

id, name, materials, max_life, view, selection_height, burning_position, energy,
food, max_hided_units, can_hide, tg_picture_id, tg_stay_id, tg_dying_id,
tg_zombie_id, tg_projectile_id, is_offensive, defence_armour, defence_protection,
offensive_aggressivity, offensive_accuracy, offensive_flags, offensive_range,
offensive_shot_time, offensive_wait_time, offensive_feed_time, offensive_scope,
offensive_shotable_seg_min_max, gun_power_min_max, gun_shot_speed –
all these items have the same meaning as the items of the same name in the <Unit> section,

•

item_type – the item denotes the unit type. One of the values is expected: “b”- building
(BUILDING UNIT), „a“- factory (FACTORY UNIT). If the value is „a“,
the items specialized for factories will additionally be loaded. (T: string, R: 1 character,
D: „b“, N: yes),

•

width – width of the unit's base in mapels. Unlike movable units,
buildings and factories do not need to have a square base. (T: byte, R: 15 >= r > 0, D: 1,
N: no),

•

height – length of the building's base in mapels. (T: byte, R: 15 >= r > 0, D: 1, N: no),

•

min_energy – the item expresses what percentage of the energy given in the energy item
the building must receive in order to perform its function without restrictions. The current energy
attributable to the building is obtained by proportionally distributing the player's energy across all
units. When the current energy drops below the value in the min_energy item, the
factory stops producing new units (production resumes automatically once at least the
minimum energy is reached). A percentage value is expected. (T: integer, R: 100 >= r >=
0, D: 100, N: no),

•

build_terrain_id - minimum and maximum terrain height where the building can be
built in the given segment determined by the item exists_segment_id. A
pair of numbers is expected. The program checks whether the loaded minimum is smaller than the maximum
(if not, the minimum is taken as the maximum). (T: integers, R: >= 0, D: 0, N: no),

•

exists_segment_id – the item determines the index of the segment in which the building is built.
(T: byte, R: number of segments = 3 > r >= 0, D: 0, N: no),


•

min_max_visible_segment_id – similar meaning to the item of the same name in the section
<Unit>, but only for the single segment determined by the item exists_segment_id. Thus
a pair of indices determining the minimum and maximum visible segment is expected, and
it is checked that the loaded minimum is smaller than the maximum. (T: byte,
R: number of segments = 3 > r >= 0, D: 0, N: no),

•

ancestor – the item determines whether the given unit type is an improvement (extension) of another unit
type (then the item's value is the unique textual identifier of that type), or
a completely new type (the item's value is „none“). If the type has a predecessor, it must have
the same size and exist in the same segment, otherwise it cannot be
built. The successor is built at the same position as its predecessor. Thus
a textual identifier of another unit type, or the text „none“, is expected. (T: string,
R: 1024, D: „none“, N: yes),

•

tg_build_id – textual identifier of the texture group of a unit under construction defined in
the data file of the corresponding race, which are displayed gradually according to the progress of construction.
The value “none” can be given, which means that the texture of a standing unit is used.
(T: string, R: 1024 characters, D: -, N: yes),

•

tg_burning_id – textual identifier of the flame texture group defined in the data
file of the corresponding race. The value “none” can be given, which means that the
tg_burning_id texture defined in the race header is used. (T: string, R: 1024 characters, D: -,
N: yes),

•

snd_selected - textual identifier of the sound (from the data file) of a unit that has
just been selected. The value „none“ can be given, which means that the
value of the item snd_building_selected defined in the race header should be used. (T: strings,
R: 1024 characters, D: -, N: yes),

•

snd_burning - textual identifier of the sound (from the data file) of a burning building. If
the value „none“ is given, the value of the item snd_burning defined
in the race header is used. (T: strings, R: 1024 characters, D: -, N: yes),

•

snd_explosion - textual identifier of the sound (from the data file) of the explosion (collapse)
of a building. In case of the value „none“, the value of the item snd_explosion
from the race header is used. (T: strings, R: 1024 characters, D: -, N: yes),

•

allowed_materials – a list of textual identifiers of materials
the building is able to accept is expected. If a non-existent identifier is given, the program
prints a warning. (T: strings, R: 1024 characters, D: -, N: no),

•

<Products> - a special property of factories. The section defines the products the
factory can produce.

<Building 0>
id "townhall"
name "Town Hall"
item_type a
width 7
height 7
materials 1200 800 0
max_life 1000
view 7
selection_height 60
burning_position 10 35


energy 0
min_energy 0
food 0
build_terrain_id 10 10
exist_segment_id 1
min_max_visible_segment_id 1 2
ancestor none
max_hided_units 5
can_hide "peasant"
tg_picture_id townhall_picture
tg_stay_id townhall_stay
tg_build_id townhall_build
tg_attack_id none
tg_dying_id none
tg_zombie_id townhall_zombie
tg_projectile_id none
tg_burning_id none
snd_selected townhall_selected
snd_burning none
snd_explosion none
allowed_materials "gold" "wood"
is_offensive false
defence_armour 20
defence_protection 0
<Products>
…
</Products>
</Building 0>

##### 3.1.3.6 Section <Products>

The section is loaded only in the case of a factory definition – the item_type item has the value „a“.
It defines the movable unit types the factory is able to produce (another way to picture it: training
soldiers in camps).
•

count - number of the factory's products. The program tries to read the given number of
<Product> sections. If it fails, loading ends with an error. (T: integer, R: >= 0, D: 0,
N: yes),

•

<Product> - section containing information about each product separately.
o product – a textual identifier of a movable unit of the given race is expected.
(T: string, R: 1024 characters, D: -, N: yes),
o product_time – time needed to create one unit, in seconds.
(T: float, R: >= 0, D: 0, N: yes).

<Products>
count 1
<Product 0>
product "paesant"
product_time 20
</Product>
</Products>


##### 3.1.3.7 Section <Sources>

The section is specific to the scheme race; player races do not contain it (if they did, it would be
ignored). It defines the detailed properties of material resources from which players in the game
obtain raw materials for their development. It contains the following items and subsections:
•
•

count - number of resource types. The program tries to read the given number of
<Source> sections. If it fails, loading ends with an error. (T: integer, R: >= 0, D: 0,
N: yes),
<Source X> - definition of one specific resource type.

<Sources>
count 4
<Source 0>
…
</Source 0>
</Sources>

##### 3.1.3.8 Section <Source>

It contains detailed definitions of the properties of a resource type. Many items have the same meaning
as the items of the same name in the <Building> section and will therefore only be listed.
•

id, name, width, height, max_life, selection_height, burning_position,
max_hided_units,
can_hide,
tg_picture_id,
tg_stay_id,
tg_zombie_id,
tg_burning_id, tg_dying_id, snd_selected, snd_explosion, snd_burning,
build_terrain_id, exists_segment_id, defence_armour, defence_protection - these
items have the same meaning as the items of the same name in the <Building> section,

•

capacity – the item determines the maximum capacity of the resource. (T: integer, R: >= 0, D: 0,
N: no),

•

offer_material - A textual identifier of the material (defined
in the corresponding scheme) that can be mined in the resource is expected. (T: string, R: 1024 characters, D: -,
N: yes),

•

renewable - the item decides whether the given resource will renew itself –
for example a forest grows by itself. (T: bool, R: true/false, D: false, N: no),

•

time_of_first_regeneration – the item is taken into account only if the resource is
renewable. It determines the time in seconds needed to renew the first unit of material
in the resource after it has been completely mined out – in the forest example it is the time until a seed sprouts from
the ground. (T: float, R: >= 0, D: 1, N: no),

•

time_of_reeneration - taken into account only for renewable resources.
It determines the time in seconds needed to renew each further unit of material
in the resource. (T: float, R: >= 0, D: 1, N: no),

•

inside_mining – the item determines whether the worker should disappear from the map when mining from this resource
(it mines the material from inside the resource - a mine), or should remain on the map (logging a forest).
(T: bool, R: true/false, D: false, N: no),

•

hideable - the item determines what should happen when the resource is mined out.
In case of the value „true“ the resource is removed from the map (the texture of the mined-out resource, however,
remains visible) and the place can be walked over. In case of the value „false“ the place is
inaccessible. (T: bool, R: true/false, D: false, N: no).


<Source 1>
id "forest"
name "Forest"
width 3
height 3
max_life 400
capacity 500
selection_height 50
burning_position 0 40
max_hided_units 0
can_hide
tg_picture_id forest_picture
tg_stay_id forest_stay
tg_zombie_id none
tg_burning_id none
tg_dying_id none
snd_selected none
snd_explosion none
snd_burning none
build_terrain_id 10 10 10 10 10 10
exist_segment_id 1
defence_armour 1
defence_protection 20
offer_material "wood"
time_of_regeneration 3
time_of_first_regeneration 600
renewable true
inside_mining false
hideable true
</Source 1>

#### 3.1.4 Description of map files

Map files are located in the "maps" subdirectory of the game's root directory and have the extension
".map". As already stated in the introduction to the chapter on configuration files, maps depend
on the scheme (because the map is composed of the scheme's terrain elements) and likewise depend on the
races whose units move in the map. The dependency of maps, schemes and races can, however,
also be viewed from the other side – from the program user, who starts the game by choosing a map
(not a scheme, as the game creator does). At the moment the map is selected, the program checks whether
the scheme given in the map exists and whether the races given in the map are correctly defined (whether they exist
and whether they are made for the scheme given in the map). If something is not in order, the game cannot start.
Map files are divided into two larger parts – the definition of the map itself by means of
scheme elements, and the definition of players = races (players on the map must have different races, because it would
not be possible to graphically distinguish two players) and their starting positions.

##### 3.1.4.1 Header

The header contains basic information about the map and auxiliary information about the map file.
•

name – name of the map. It is not used as an identifier (that is the file name without
extension), but it is displayed when selecting a map in the game's main menu. (T: string, R: 1024
characters, D: „“, N: no),


•

author – name of the file's author. It is not used in the program; it is just extra information.
(T: string, R: 1024 characters, D: „“, N: no),

•

width – width of the map in mapels – the smallest addressable units of the map.
(T: byte, R: maximum map size = 240 >= r >= 1, D: 1, N: yes),

•

height – length (height) of the map in mapels. (T: byte, R: maximum map size =
240 >= r >= 1, D: 1, N: yes),

•

scheme – a textual identifier of the scheme for which the map is created is expected. When
selecting the map from the menu, it is checked whether the given scheme exists and whether it is correctly
defined. (T: string, R: 1024 characters, D: „“, N: yes).

name "Trial map"
author "PP team"
width 80
height 70
scheme "plastic"

##### 3.1.4.2 Section <Players>

The first of the two large parts of the map definition – it defines the number of players, their starting
positions and the races they can choose. Each map has a maximum number of players, but for
all of them to actually be able to play, a sufficient number of starting positions and also
a sufficient number of races must be defined. The number of players who can play on the map is the minimum of: the number of races,
the number of starting positions, the maximum number of players. The section has the following items and
subsections:
•
•
•

max_count – maximum allowed number of players on the map. (T: integer,
R: maximum number of players = 8 >= r >= 0, D: 0, N: yes),
<Start Points> - defines all possible starting positions of players in the map,
<Races> - list of all races that can be used on the map.

<Players>
max_count 2
<Start Points>
…
</Start Points>
<Races>
…
</Races>
</Players>

##### 3.1.4.3 Section <StartPoints>

The section defines all possible starting positions on the map. After the game starts, the program
randomly assigns them to players so that no two start the game at the same position. This ensures
a certain unrepeatability of each game.
•

count – number of starting positions the program tries to read. If it
fails, loading ends with an error. (T: integer, R: >= 0, D: 0, N: yes),

•

start_point_X – defines the coordinates of a starting position in the map. The program checks whether
the given position is within the map. To limit the file size, the parameters of a starting position
are given on one line. 2 numbers are expected:


o x – x coordinate of the position. (T: integer, R: width >= r >= 0, D: 0, N: yes),
o y - y coordinate of the position. (T: integer, R: height >= r >= 0, D: 0, N: yes).
<Start Points>
count 1
start_point_0 65 48
</Start Points>

##### 3.1.4.4 Section <Races>

The section defines all allowed races. For the reasons already given, players on
the map must have different races. Besides the "real" races, the so-called "scheme" race is also defined here,
by means of which mainly resources get into the map – they are thus owned by the "scheme" player.
The other races can own only buildings and movable units.
•

count – number of races (<Race> sections) the program tries to read. If it
fails, loading ends with an error. These are only the races chosen by real players.
(T: integer, R: >= 0, D: 0, N: yes),

•

<Race> - the section defines all units and buildings the player of the given race will have
at the start of the game,

•

<SchemeRace> - the section defines the units, buildings and resources owned
by the scheme player. This section occurs only once.

<Races>
count 2
<Race 0>
…
</Race 0>
<SchemeRace>
…
</SchemeRace>
</Races>

##### 3.1.4.5 Sections <Race> and <SchemeRace>

To avoid monotonous game starts, the program allows defining different initial
player states by defining a different number of other, or differently arranged, units.
After the game starts, the program randomly chooses one of the defined initial player states
and places the units into the map relative to a randomly chosen starting position (see section
<Start Points>). All starting positions of the player's units and buildings are thus given
relative to a "fictitious starting position", which is mapped onto the real starting position
of the map. Another difference between a real and the scheme player is that the scheme player
has a single starting point, which is fixed at the origin of the map (position [0,0]). The positions
of the scheme player's units, buildings and resources are thus given relative to position [0,0] – i.e.
absolutely.
•

name – textual identifier of the race. The file of this race must exist and must be
correct, otherwise the program ends with an error. (T: string, R: 1024 characters, D: „“, N: yes),

•

<Sets> - subsection defining all initial states of the player who chose the given
race. The scheme race definition does not have this section; it directly contains the sections <Units>,
<Buildings>, <Sources> (see below).


<Race>
name "human-yellow"
<Sets>
…
</Sets>
</Race 1>

Map

Starting position
Relative starting
position

Initial
arrangement of units

##### 3.1.4.6 Sections <Sets> and <Set>

Both sections serve more or less as wrappers for the definitions of the player's initial states.
•

count – the item has the standard meaning – it determines the number of <Set> sections the program
tries to read. If it fails, loading ends with an error. (T: integer, R: >= 0,
D: 0, N: yes),

•

<Set> - definition of one specific starting state of the player.
o init_materials_amount – initial material amounts of the player for the given
starting state. As many numbers are expected as materials were defined
in the corresponding scheme. (T: integes, R: >= 0, D: 0, N: yes),
o <Units>, <Buildings>, <Sources> - unit definitions. The <Sources> section is
expected only in the scheme race definition, otherwise it is ignored.

<Sets>
count 1
<Set 0>
init_materials_amount 1500 1000 1000
<Units>
…
</Units>
<Buildings>
…
</Buildings>


<Sources>
…
</Sources>
</Set 0>
</Sets>

##### 3.1.4.7 Sections <Units>, <Buildings> and <Sources>

The section contains definitions of units, buildings and resources of one starting state of the player.
•

count – determines the number of items of the corresponding section the program tries to read. If
it fails, loading ends with an error. (T: integer, R: >= 0, D: 0, N: yes),

•

unit_X – defines the unit type from the corresponding race, its position in the map, orientation
and initial life. 6 numbers are expected:
o unit_id – identifier of the unit type (the number Z from the definition of the section <Unit Z>
in the scheme),
o x, y, z – position of the lower left corner of the unit in the map. (T: integer,
R: maximum map size >= r >= 0, D: 0, N: yes),
o direction – orientation of the unit (a number from the interval <0,7>
representing the direction is expected). (T: integer, R: 7 >= r >= 0, D: 0, N: yes),
o life – current life expressed as a percentage of the maximum life of the given
unit type. (T: integer, R: 100 >= r >= 0, D: 0, N: yes).

•

building_X – defines the building type from the corresponding race, its position in the map and initial
life. 4 numbers are expected:
o building_id – identifier of the building type (the number Z from the definition of the section <Building
Z> in the scheme),
o x, y – position of the lower left corner of the building in the map. (T: integer,
R: maximum map size >= r >= 0, D: 0, N: yes),
o life – current life expressed as a percentage of the maximum life of the given
building type. (T: integer, R: 100 >= r >= 0, D: 0, N: yes).

•

source_X - defines the resource type from the corresponding race, its position in the map, initial life and
the initial amount of material in the resource (current capacity). 5 numbers are expected:
o source_id – identifier of the resource type (the number Z from the definition of the section <Source Z>
in the scheme),
o x, y – position of the lower left corner of the resource in the map. (T: integer, R: maximum
map size >= r >= 0, D: 0, N: yes),
o life – current life expressed as a percentage of the maximum life of the given
resource type. (T: integer, R: 100 >= r >= 0, D: 0, N: yes),
o material_balance – absolute expression of the current amount of material in the resource.
(T: integer, R: >= 0, D: 0, N: yes).

<Units>
count 1


unit_0 "footman" 9 0 1 0 70
</Units>
<Buildings>
count 2
building_0 "fort" 0 0 80
building_1 "farm" -3 0 100
</Buildings>
<Sources>
count 2
source_0 "goldmine" 33 5 75 50000
source_1 "goldmine" 70 25 50 50000
</Sources>

##### 3.1.4.8 Section <Segment>

Segments represent the second large part of the map definition. Three
sections are expected in map files: <Segment 0>, <Segment 1> and <Segment 2>, which express the individual layers of the map.
This achieves the "three-dimensionality" of the space in which the players' units can move.
The individual segments represent, in order, the subsurface layer (underground), the surface (ground)
and the above-surface layer (airspace). Each section, by means of fragments defined
in the scheme, describes the "surface" of the layers and thus the accessibility of a field – a mapel (the smallest
addressable unit of the map) for the player's individual units. In addition, objects and layers defined in the scheme
can be placed into the map.
•

<Fragments> - subsection containing a list of fragments mapped onto the surface
of the map,

•

<Layers> - subsection containing a list of layers inserted into the map,

•

<Objects> - subsection containing instances of objects added to the map.

<Segment 1>
<Fragments>
…
</Fragments>
<Layers>
…
</Layers>
<Objects>
…
</Objects>
</Segment 1>

##### 3.1.4.9 Section <Fragments>

The <Fragments> section is the most important section of the whole map configuration file.
It contains the mapping of fragments defined in the scheme onto the map surface and thus determines which
map field – mapel will be accessible to a unit and how fast the unit will
move across it. It has the following items:
•

count – number of fragments. The program tries to read the given number of items
fragment_X. If it fails, loading ends with an error. (T: integer, R: >= 0,
D: 0, N: yes),

•

fragment_X – defines the type of a fragment defined in the scheme and its coordinates
in the map. The program checks whether the whole fragment is placed within the map. If not, into


the map it is not added, and the empty place is filled with the corresponding number of default
fragments of size 1x1 mapel from the scheme. Two fragments must not overlap. If
an erroneous fragment is given, it will not be added to the map. To limit the file size,
the fragment parameters are given on one line (clearly visible in the
example). Three numbers are expected:
o

type – identifier of the fragment type (the number X of the section <Fragment X> from the scheme).
(T: integer, R: >= 0, D: 0, N: yes),

o

x – x coordinate of the lower left corner of the fragment in the map. (T: integer,
R: >= 0, D: 0, N: yes),

o

y - y coordinate of the lower left corner of the fragment in the map. (T: integer,
R: >= 0, D: 0, N: yes).

<Fragments>
count 200
fragment_0 0 70 65
fragment_1 0 75 65
</Fragments>

##### 3.1.4.10 Section <Layers>

The section determines which layer types from the scheme are used in the map and defines their placement.
Layers locally change the terrain height.
•

count – number of layers to load. The program tries to read the given number of items
layer_X. If it fails, loading ends with an error. (T: integer, R: >= 0, D: 0,
N: yes),

•

layer_X – the item defines the type of a layer defined in the scheme and its coordinates in the map.
The program checks whether the whole layer lies within the map – if not, it will not be added to the map.
Layers may overlap and the terrain height of a given mapel is determined by the most recently
added layer. Like fragments, layers have their parameters on one line.
Three numbers are expected:
o type – identifier of the layer type (the number X of the section <Layer X> from the scheme).
(T: integer, R: >= 0, D: 0, N: yes),
o

x – x coordinate of the lower left corner of the layer in the map. (T: integer, R: >=
0, D: 0, N: yes),

o

y - y coordinate of the lower left corner of the layer in the map. (T: integer, R: >=
0, D: 0, N: yes).

<Layers>
count 1
layer_0 0 25 0
</Layers>

##### 3.1.4.11 Section <Objects>

The section contains a list of objects to be added to the map. An object has similar
properties to a layer. They differ in the way they are drawn and in that they must not overlap
(this is explained in detail in the scheme definition). Objects are loaded from the map the same way as
layers, except that it is checked that they do not overlap. Three numbers are expected: type, x, y


<Objects>
count 4
object_0 0 70 45
</Objects>

### 3.2 Data files
Data files bundle the audio and video records used in the game Dark Oberon. The individual
configuration files of the game can refer to these records by means of textual
identifiers.
A data file contains two types of records that can be referred to: texture groups
and sounds. Texture groups are subject to exact rules depending on the purpose for which the group will be
used. The following chapters describe the list of these rules by reference type.
A common rule for all groups is that at least one texture must exist in the group.
Textures can be animated.
A detailed description of data files can be found in the Data Editor documentation.

#### 3.2.1 Races

The data file for races can contain the following types of texture groups (divided by the name of
the item in the race configuration file):
•

tg_food_id – the group contains two textures. The first is the food icon with a frame size
of 15x12. With different dimensions the texture will be deformed. The second
texture is used as a sign in case of a food shortage (e.g. when building
units in factories),

•

tg_energy_id – same as for tg_food_id,

•

tg_material0_id, tg_material1_id, tg_material2_id, tg_material3_id – same as
for tg_food_id,

•

tg_burning_id – the individual textures represent the degree of fire from the smallest to
the largest,

•

Units
o tg_picture_id – the group contains one texture with the picture of the unit.
Dimensions of 50x40 pixels are assumed. A picture with other dimensions will be
deformed,
o tg_stay_id, tg_anchor_id, tg_move_id, tg_rotating_id, tg_attack_id –
contain exactly one or eight textures, one for each direction of the unit,
o tg_projectile_id – contains exactly one or eight textures, one for each direction of the
projectile,
o tg_dying_id – similar to tg_stay_id. In addition, the rule applies that the length
of the animation is used for the length of the state in which the unit is,
o tg_zombie_id – similar to tg_stay_id. In addition, the length of the texture
animation is adjusted according to the length of the state,
o tg_burning_id – the individual textures represent the degree of fire from the smallest
to the largest,


•


Buildings
o tg_picture_id, tg_projectile_id, tg_burning – same as for units,
o tg_stay_id – the individual textures represent the degree of destruction of the building from full
life down to zero life,
o tg_build_id – the individual textures represent the stage of building construction from
the foundations up to the stage just before the building is completed. For the last stage
of construction the first texture from tg_stay_id is used,
o

#### 3.2.2 tg_dying_id, tg_zobie_id – exactly one texture. The same rules as for units apply
to animation lengths.

Schemes

The data file for a scheme contains both records connected with the scheme itself and
records of the scheme race. The scheme race uses the same record types as a regular race;
in addition, it can contain texture groups for material resources.
Types of texture groups by reference:
•

Fragments – if the group contains several textures, a random one is chosen to display the fragment.
For correct rendering, all textures must have the type
"Terrain Fragment" set,

•

Objects – similarly to fragments, a random texture from the group is chosen
for display,

•

Resources:
o tg_stay_id – the group contains either one texture or an even number of textures,
where the individual pairs of consecutive textures display the resource according to
the remaining material in it (from a full resource to an empty one).
The first texture of a pair represents the resource in its normal state, the second in the state when
some unit is mining from the resource,
o tg_picture_id, tg_burning, tg_dying, tg_zombie – the same rules as
for the race's buildings.


## 4 Map Editor
Peter Knut


### 4.1 Introduction
The Map Editor program allows creating and editing map configuration files used
in the game Dark Oberon. This editor is not complete; it is focused only on editing map surfaces.
Other parts of the map (such as resources or players' starting positions) must be added
manually. If the map already contains these parts, they will be preserved after editing.
Another limitation of the editor is that it assumes the same size of all fragments in
all segments of the map. (An explanation of the terms mapel, fragment and map segment is
given in the documentation on creating your own map in the game Dark Oberon.)
For better distinction of displayed fragments and orientation in the map, it is possible to define
a color scheme in which the corresponding color and fragment name are given for the individual
fragment numbers. This scheme can be saved to disk in a separate file and opened again.
After the application starts, a new empty color scheme and a maximum-size map
with the standard fragment size of 5 mapels are created automatically.

### 4.2 Main window
The main window of the application is divided into two basic parts. On the left side there is
the fragment table, which also shows the color scheme; the rest of the window is formed by the panel
displaying the map.
Of the map itself, the editor always displays the selected segment in the form of a regular grid.
Each grid cell represents one fragment. The individual cells (fragments) are
distinguished from each other by number and color. Segments can be switched in the lower left part of the window.

Map

Fragment
table
Displayed segment
Figure 16: Main application window.


### 4.3 Map
#### 4.3.1 Working with files

Using the commands of the Map menu you can create a new map,
open an existing map or save the open map. When
creating a new map, the "New map" dialog is shown
with the map properties. Here you need to enter the desired
map dimensions and the fragment size. Both values are
in mapels. The fragment size must not be larger than
either of the map dimensions. If the map dimensions
are not a multiple of the fragment size, they will be reduced to
the nearest multiple.
Similarly, when opening a map you need to enter in the
"Fragments size" dialog the fragment size that the map
uses.

Figure 17: "New map" dialog

The map properties can be changed at any time in the
"Map properties" dialog (menu command Map/Properties).
Exactly one map can be open and edited
at a time.

#### 4.3.2 Editing

Fragments are inserted into the map by left-clicking
on the corresponding place in the displayed grid.
The fragment to be inserted can be changed in the fragment
table.

Figure 18: "Fragments size" dialog

The right mouse button inserts the fragment with number zero.

### 4.4 Color scheme
#### 4.4.1 Working with files

The commands of the Scheme menu are used to create
a new color scheme, open an existing
scheme or save the open scheme.
The scheme in use can be changed at any time
while editing the map.
Exactly one scheme can be open at a time.

#### 4.4.2 Editing

The color scheme is edited using the
"Scheme definition" dialog (command Scheme/Definition).
The segment field specifies the desired segment. When
the fragment number is changed, its current name and
color are displayed automatically. These two values


Figure 19: "Scheme definition" dialog


can be changed (the color is changed by clicking the rectangle showing the fragment's color).
The maximum number of fragments is 256.
The Done button ends editing of the scheme and closes the dialog.

### 4.5 Keyboard shortcuts
Ctrl+N
Ctrl+O
Ctrl+S
Shift+Ctrl+N
Shift+Ctrl+O
Shift+Ctrl+S
F1

Creates a new map.
Opens an existing map.
Saves the open map under a new name.
Creates a new color scheme.
Opens an existing color scheme.
Saves the open color scheme under a new name.
Shows the About window.


## 5 Data Editor
Peter Knut


### 5.1 Introduction
The Data Editor program allows creating and editing data files used in the game Dark
Oberon. A data file (*.dat) contains two basic types of records: textures (image data)
and sounds.
Textures are organized in groups, where each texture belongs to exactly one group.
A group cannot contain other groups. The number of textures in a group is unlimited.
Sound records are not organized; their number is likewise unlimited.

### 5.2 Main window
The main window of the application is divided into two basic parts. On the left side there is
a tree diagram showing the structure of the data file; the rest of the window is formed by the panel for
editing the parameters of the individual records.

Parameter panel

File structure
diagram
Figure 20: Main application window.

### 5.3 Working with files
Using the commands of the File menu you can create a new empty file, open an existing file, save
and close the open file. When closing an unsaved file, a prompt to save is shown.
Only one file can be open and edited at a time.
The currently supported data file version is 3. The editor can also open files
in previous versions, which will be automatically converted to the current version.


### 5.4 Editing data
To be able to add data items, the desired data type must first be activated.
These types are activated and deactivated with the commands: Data/Textures for textures and Data/Sounds for
sounds. After activation, the commands of the Data menu can be used to add, delete and move individual
records. On deactivation, all records belonging to the corresponding type are deleted.
When adding a record, the user is automatically prompted to choose the source file for the texture
or sound. After loading, the source file becomes part of the data file.
Selecting an item in the diagram shows its editable parameters on the panel.

#### 5.4.1 Texture group

Here only the group name can be changed. This name can contain an unlimited number of
characters.

#### 5.4.2 Texture

Textures in the game Dark Oberon are animated. Animation is achieved by dividing the source image
of the texture into individual frames. All frames have the same size and are
arranged in a regular grid. An example of such a texture is shown in the figure.
In the Frames field you can set the horizontal and vertical number of frames in the texture and the animation length
in milliseconds. The number of frames is in the interval <1, 100>, the animation length in the interval <0,
10000>.
The parameters X, Y in the Base point field define the point in the frame that determines the origin of the coordinate
system used for rendering. By default this origin is at the point [0, 0]. With non-zero
values the texture is locally shifted. E.g. with the values [10, 5] the texture
is shifted 10 pixels to the left and 5 pixels down. A point outside the frame size can also be given.
Allowed values are in the range <-1024, 1024>.
The texture type determines the way the texture is rendered. If
the texture is intended for a terrain fragment, the type must be set
to Texture of terrain fragment. Otherwise the type is set to
Normal texture.
The Data field contains information about the size of the source file
and buttons for loading a new file and saving the file to
disk. The source file for a texture is an image in
TGA format. The maximum image size is 1024x1024
pixels.

Figure 21: Example of an animated
texture

The texture name is unlimited.

#### 5.4.3 Sound record

The following formats can be used as the source file for a sound: WAV, MP2, MP3, OGG, RAW,
MOD, S3M, XM, IT, MID, RMI, SGT. The format used is shown in the Format field.
A sound record can be of two types:
•

Sample – the source file is stored in memory when the data file is loaded, so
access to it is very fast. This type is suitable for short and frequent sounds,


•


Stream – the source file is not loaded in advance. During playback it is read directly from disk.
Suitable for very long and infrequent sounds, e.g. for background music in the game.

The name of a sound record is unlimited.

### 5.5 Texture export and import
The commands File/Export/All textures and File/Import/All textures are used for bulk export
and import of texture source files.
Exported files have the form <given name><generated number>.tga. Numbering starts
at 1000 and increases by one for each following image. When importing, it is enough to choose
one of the files of this form. The number of input images must match the number of textures
in the data file.

### 5.6 Keyboard shortcuts
Ctrl+N
Ctrl+O
Ctrl+S
F1
Del

Creates a new data file.
Opens an existing data file.
Saves the open file.
Shows the About window.
Deletes the item selected in the file diagram.
