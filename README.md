Warning!!!
Xiri is 80% coded by human and 20% coded by ai (config file + some bug fixes was done by ai) so if you avoid to use ai coded stuff, js know that even i use ai in development.  


Xiri-wm is a x11 based window manager that written in c++ and xcb. It's inspired by niri-wm and have similar mechanic that imitating scrolling.  

Before using xiri, u have to remember that it was made by unexperienced developer and it may contain bugs and unoptimized code that i will make better in future.  

Packages that you should have before using xiri:  
1.rofi  
2.xrandr  
3.feh  

  
How to install xiri:  
1.Download the zip file from github  
2.Unzip it  
3.Type "sudo make sddm"  
4.Type "make config". And then configure config.ini at the ~/.config/xiri (You can also js preconfigure the config.ini and then type make config, because its just copies file to the directory)  
5.Save changes at the config file    
6.Test it by typing "make xephyr" (optionally)  
7.Type "sudo make install"  
8.Log out from your session  
9.Enter xiri and enjoy  

Credits:  
Main coding, bug fixing, testing - Semga (me)  
Assisting me and giving me a good start by recoding my wm at first days - neko-qt (he is at contributors)
