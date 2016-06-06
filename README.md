# MultiFive - multiplayer for GTA V based on FiveM
Original code belongs to CitizenFX project. Licensed under GPUv3.

    (C) Multifive Team and CitizenFX project / 2016 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

## How to compile MultiFive core?
##### Install VS and needed components for compiling project:
- [Visual Studio 2015 Community edition](https://www.visualstudio.com/ru-ru/downloads/download-visual-studio-vs.aspx) 
- [Boost library 1.57](https://sourceforge.net/projects/boost/files/boost/1.57.0/)
- [depot tools](https://www.chromium.org/developers/how-tos/install-depot-tools) dont forget to add that $PATH

##### Open CMD and type:

```bash
cd projectfoldername
"%VS120COMNTOOLS%....\vc\vcvarsall.bat"
gclient config --unmanaged https://github.com/multifive/multicore.git
gclient sync
#  now it will setup enviroment via premake5
#  download premake5 from here - http://premake.github.io/download.html#v5 and
#  place it into the project directory
cd multicore
git checkout portability-five
gclient sync
..\build\premake\bin\release\premake5 --game=five vs2015
```

Now you can open multicore\build\five\CitizenMP.sln with Visual Studio 2015
It will take a while to index. Try to build the solution! 

## How to start MultiFive?

Open folder `multicore\bin\five\debug` and put there files [from that repository](https://github.com/multifive/multicache/tree/master/caches/fivem) and then launch FiveM.exe.

If you get any errors while compiling or doing something about that just leave your problem here: [Issues](https://github.com/multifive/multicore/issues). Thanks! :v:
