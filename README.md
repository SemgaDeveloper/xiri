XIRI-WM IS IN VERY EARLY DEVELOPMENT BY UNEXPERIENCED PERSON, SO YOU WILL MEET BUGS THAT WILL BE FIXED LATER. I'M ALREADY BUSY ON IMPROVING USER EXPERIENCE BECAUSE EVENTS NOW SO BUGGY. I ALREADY FIXED ISSUES WHEN WM COULD SCROLL WINDOWS BY ITLSEF + IMPROVED SCROLLING A LITTLE BIT BUT IT STILL CAN BE REWORKED IN FUTURE. ALSO CURRENTLY I'M BUSY ON ADDING MORE FEATURES.
Thank you for supporting and testing xiri-wm! Because for now it's so unfinished and in active development.
All credits to me and my contributor - neko-qt, special thank to him, because he reworked whole code of my wm and gave me idea how to continue, if he wasn't here, i will probably still struggle trying to do usable keyboard input, windows monocle tiling and other many things.
How to get started:
To get started, clone the repository by using git and then change directory to xiri. After all you can type "make help" to see what make file can do (Credits to neko-qt, he made make file).
If you want to run window manager isolated, then type "make xephyr" (xephyr installed required). Else if you want to run it directly and use, check the installation tutorial at the bottom of README.md 
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
