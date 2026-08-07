XIRI-WM IS IN VERY EARLY DEVELOPMENT BY UNEXPERIENCED PERSON, SO YOU WILL MEET BUGS THAT WILL BE FIXED LATER. I'M ALREADY BUSY ON IMPROVING USER EXPERIENCE BECAUSE EVENTS NOW SO BUGGY AND WM SOMETIMES SCROLLS WINDOWS BY ITSELF + I WILL MAKE IT MORE CUSTOMIZABLE IN FUTURE 

Thank you for supporting and testing xiri-wm! Because for now it's so unfinished and in active development.
To get started, clone the repository by using git and then change directory to xiri. After all you can type "make help" to see what make file can do (Credits to neko-qt, he made make file and recoded almost everything in my window manager). But the make file still has some unfinished/not working stuff that will be fixed soon.
If you want to run window manager, type "make xephyr" (xephyr installed required). 
Keybinds:
Mod + Enter - open xterm (xterm installed required)
Mod + D - open rofi (rofi installed required)
Mod + Tab - switch between windows
Mod + LeftArrow - scroll left
Mod + RightArrow - scroll right
Mod + Q - close focused window

Installation tutorial:
1.Download zip file of xiri
2.Unzip it
3.Configure it through xiri.cpp (add basic autostarts - wallpapers, xrandr resolution, bars and other things), don't worry, there will be special config file in future that will make xiri's customization a lot easier.
4.Type "sudo make sddm" or "doas make sddm"
5.Type "sudo make install" or "doas make install"
6.Log out from your session and check list of your window managers, if everything done rightly, then you should be able to run into xiri.
